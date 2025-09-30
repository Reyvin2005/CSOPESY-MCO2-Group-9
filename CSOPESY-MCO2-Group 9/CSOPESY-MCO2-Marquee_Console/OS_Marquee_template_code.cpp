#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

// --- Color Codes ---
namespace Colors {
    const std::string RESET = "\033[0m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
    const std::string BRIGHT_RED = "\033[91m";
    const std::string BRIGHT_GREEN = "\033[92m";
    const std::string BRIGHT_YELLOW = "\033[93m";
    const std::string BRIGHT_BLUE = "\033[94m";
    const std::string BRIGHT_CYAN = "\033[96m";
    const std::string BRIGHT_WHITE = "\033[97m";
    const std::string BOLD = "\033[1m";
}

// --- Layout constants (1-based coords) ---
constexpr int MARQUEE_WIDTH = 41;       // inner width of marquee box
constexpr int MARQUEE_TOP_ROW = 2;      // box top border
constexpr int MARQUEE_TEXT_ROW = 3;     // inner text row (where scrolling appears)
constexpr int MARQUEE_BOTTOM_ROW = 4;   // box bottom border
constexpr int STATUS_ROW = 6;           // status shows here
constexpr int HELP_ROW = 7;             // help messages show here
constexpr int PROMPT_ROW = 20;          // input prompt locked here
constexpr int PROMPT_COL = 1;           // column where prompt starts (">> ")
constexpr int SCREEN_WIDTH = 120;       // terminal width

// --- Shared state ---
std::atomic<bool> is_running{ true };
std::atomic<bool> marquee_running{ false };
std::string marquee_text = " Welcome to CSOPESY Marquee! ";
std::atomic<int> marquee_speed{ 200 };  // ms
std::atomic<int> marquee_position{ 0 };
std::mutex marquee_state_mutex;

std::mutex prompt_mutex;
std::string prompt_display = ">> ";

// --- Terminal helpers ---
void enable_ansi_on_windows() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) return;
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, mode);
#endif
}

// --- 1-based column/row ---
void gotoxy(int col, int row) {
    std::cout << "\033[" << row << ";" << col << "H";
}
void clear_screen() {
    std::cout << "\033[2J\033[H" << std::flush;
}
void clear_line(int row, int width = SCREEN_WIDTH) {
    gotoxy(1, row);
    std::cout << std::string(width, ' ') << std::flush;
}

inline void save_cursor() { std::cout << "\033[s"; }
inline void restore_cursor() { std::cout << "\033[u"; }

// --- Draw the static UI once (title, marquee box, initial status/help, prompt) ---
void display_static_ui() {
    clear_screen();

    // Title (row 1)
    gotoxy(1, 1);
    std::cout << Colors::BOLD << Colors::BRIGHT_BLUE
        << "========= CSOPESY Marquee Console ========="
        << Colors::RESET;

    // Marquee box
    gotoxy(1, MARQUEE_TOP_ROW);
    std::cout << Colors::MAGENTA << "+" << std::string(MARQUEE_WIDTH, '-') << "+" << Colors::RESET;
    gotoxy(1, MARQUEE_TEXT_ROW);
    std::cout << Colors::MAGENTA << "|" << Colors::RESET << std::string(MARQUEE_WIDTH, ' ') << Colors::MAGENTA << "|" << Colors::RESET;
    gotoxy(1, MARQUEE_BOTTOM_ROW);
    std::cout << Colors::MAGENTA << "+" << std::string(MARQUEE_WIDTH, '-') << "+" << Colors::RESET;

    // Status (row STATUS_ROW)
    gotoxy(1, STATUS_ROW);
    std::cout << Colors::BRIGHT_WHITE << "Status: " << Colors::RESET
        << Colors::RED << "Stopped" << Colors::RESET
        << Colors::BRIGHT_WHITE << " | Speed: " << Colors::YELLOW
        << marquee_speed.load() << "ms" << Colors::RESET;

    // Help line (row HELP_ROW)
    gotoxy(1, HELP_ROW);
    std::cout << Colors::BRIGHT_GREEN << "Type 'help' for available commands." << Colors::RESET;

    // Prompt (PROMPT_ROW)
    gotoxy(PROMPT_COL, PROMPT_ROW);
    std::cout << Colors::CYAN << prompt_display << Colors::RESET << std::flush;
}

void update_status_line() {
    save_cursor();
    clear_line(STATUS_ROW);
    gotoxy(1, STATUS_ROW);
    std::cout << Colors::BRIGHT_WHITE << "Status: " << Colors::RESET;
    if (marquee_running.load())
        std::cout << Colors::BRIGHT_GREEN << "Running" << Colors::RESET;
    else
        std::cout << Colors::RED << "Stopped" << Colors::RESET;
    std::cout << Colors::BRIGHT_WHITE << " | Speed: " << Colors::YELLOW
        << marquee_speed.load() << "ms" << Colors::RESET << std::flush;
    restore_cursor();
}

void show_help_line() {
    save_cursor();
    // Clear enough lines for multi-line help (e.g., HELP_ROW to HELP_ROW+7)
    for (int row = HELP_ROW; row <= HELP_ROW + 7; ++row) {
        clear_line(row);
    }
    // Print help starting at HELP_ROW
    gotoxy(1, HELP_ROW);
    std::cout << Colors::BOLD << Colors::BRIGHT_CYAN << "Available Commands:" << Colors::RESET << "\n";
    gotoxy(1, HELP_ROW + 1);
    std::cout << Colors::BRIGHT_YELLOW << "  help" << Colors::WHITE << " - displays the commands and its description\n";
    gotoxy(1, HELP_ROW + 2);
    std::cout << Colors::BRIGHT_YELLOW << "  start_marquee" << Colors::WHITE << " - starts the marquee animation\n";
    gotoxy(1, HELP_ROW + 3);
    std::cout << Colors::BRIGHT_YELLOW << "  stop_marquee" << Colors::WHITE << " - stops the marquee animation\n";
    gotoxy(1, HELP_ROW + 4);
    std::cout << Colors::BRIGHT_YELLOW << "  set_text" << Colors::WHITE << " - accepts a text input and displays it as a marquee\n";
    gotoxy(1, HELP_ROW + 5);
    std::cout << Colors::BRIGHT_YELLOW << "  set_speed" << Colors::WHITE << " - sets the marquee animation refresh in milliseconds\n";
    gotoxy(1, HELP_ROW + 6);
    std::cout << Colors::BRIGHT_RED << "  exit" << Colors::WHITE << " - terminates the console\n" << Colors::RESET << std::flush;
    restore_cursor();
}

// --- Marquee thread: updates ONLY the MARQUEE_TEXT_ROW ---
void marquee_thread_func(int display_width) {
    while (is_running) {
        if (marquee_running.load()) {
            std::string text_copy;
            {
                std::lock_guard<std::mutex> lock(marquee_state_mutex);
                text_copy = marquee_text;
            }

            if (!text_copy.empty() && display_width > 0) {
                std::string buffer = text_copy + std::string(display_width, ' ');
                int len = static_cast<int>(buffer.size());
                int pos = marquee_position.load();
                if (pos >= len) pos = 0;

                std::string visible;
                if (pos + display_width <= len) {
                    visible = buffer.substr(pos, display_width);
                }
                else {
                    int first = len - pos;
                    visible = buffer.substr(pos, first) + buffer.substr(0, display_width - first);
                }

                // print inside the marquee box without disrupting user typing
                save_cursor();
                gotoxy(2, MARQUEE_TEXT_ROW); // column 2 inside left border, row MARQUEE_TEXT_ROW
                std::cout << Colors::WHITE << std::setw(display_width) << std::left << visible << Colors::RESET << std::flush;
                restore_cursor();

                marquee_position.store((pos + 1) % len);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(marquee_speed.load()));
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

// --- Command helpers ---
void set_marquee_text(const std::string& text) {
    std::lock_guard<std::mutex> lock(marquee_state_mutex);
    marquee_text = text.empty() ? " " : text;
    marquee_position.store(0);
}

void set_marquee_speed(int spd) {
    if (spd > 0) marquee_speed.store(spd);
    update_status_line();
}

// --- Main ---
int main() {
    enable_ansi_on_windows();

    // draw UI once
    display_static_ui();

    // start marquee thread
    std::thread marquee_thread(marquee_thread_func, MARQUEE_WIDTH);

    enum CommandState { NORMAL, WAITING_TEXT, WAITING_SPEED };
    CommandState state = NORMAL;

    while (is_running) {
        // prepare the prompt on PROMPT_ROW and compute where user input should start
        {
            std::lock_guard<std::mutex> lock(prompt_mutex);
            // clear and print prompt
            clear_line(PROMPT_ROW);
            gotoxy(PROMPT_COL, PROMPT_ROW);
            std::cout << Colors::CYAN << prompt_display << Colors::RESET << std::flush;
        }

        int input_col;
        {
            std::lock_guard<std::mutex> lock(prompt_mutex);
            input_col = PROMPT_COL + static_cast<int>(prompt_display.size());
        }

        // position cursor for input
        gotoxy(input_col, PROMPT_ROW);

        // read a line
        std::string line;
        if (!std::getline(std::cin, line)) { // EOF or error
            is_running = false;
            break;
        }

        // clear the typed input from prompt row so it doesn't remain visible after Enter
        clear_line(PROMPT_ROW);
        {
            std::lock_guard<std::mutex> lock(prompt_mutex);
            gotoxy(PROMPT_COL, PROMPT_ROW);
            std::cout << Colors::CYAN << prompt_display << Colors::RESET << std::flush;
        }

        // trim
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            if (!s.empty()) s.erase(s.find_last_not_of(" \t\r\n") + 1);
            };
        trim(line);
        if (line.empty()) continue;

        if (state == WAITING_TEXT) {
            set_marquee_text(line);
            state = NORMAL;
            {
                std::lock_guard<std::mutex> lock(prompt_mutex);
                prompt_display = ">> ";
            }
            update_status_line();
            continue;
        }
        if (state == WAITING_SPEED) {
            try {
                int v = std::stoi(line);
                if (v > 0) set_marquee_speed(v);
            }
            catch (...) { /* ignore */ }
            state = NORMAL;
            {
                std::lock_guard<std::mutex> lock(prompt_mutex);
                prompt_display = ">> ";
            }
            update_status_line();
            continue;
        }

        // Normal commands
        if (line == "help") {
            show_help_line();
        }
        else if (line == "start_marquee") {
            marquee_running = true;
            update_status_line();
            for (int row = HELP_ROW; row <= HELP_ROW + 7; ++row) clear_line(row);
            gotoxy(1, HELP_ROW);
            std::cout << Colors::BRIGHT_GREEN << "Type 'help' for available commands." << Colors::RESET << std::flush;
        }
        else if (line == "stop_marquee") {
            marquee_running = false;
            update_status_line();
            for (int row = HELP_ROW; row <= HELP_ROW + 7; ++row) clear_line(row);
            gotoxy(1, HELP_ROW);
            std::cout << Colors::BRIGHT_GREEN << "Type 'help' for available commands." << Colors::RESET << std::flush;
        }
        else if (line == "set_text") {
            state = WAITING_TEXT;
            {
                std::lock_guard<std::mutex> lock(prompt_mutex);
                prompt_display = "Enter text for marquee: ";
            }
            for (int row = HELP_ROW; row <= HELP_ROW + 7; ++row) clear_line(row);
            gotoxy(1, HELP_ROW);
            std::cout << Colors::BRIGHT_GREEN << "Type 'help' for available commands." << Colors::RESET << std::flush;
        }
        else if (line == "set_speed") {
            state = WAITING_SPEED;
            {
                std::lock_guard<std::mutex> lock(prompt_mutex);
                prompt_display = "Enter speed in ms: ";
            }
            for (int row = HELP_ROW; row <= HELP_ROW + 7; ++row) clear_line(row);
            gotoxy(1, HELP_ROW);
            std::cout << Colors::BRIGHT_GREEN << "Type 'help' for available commands." << Colors::RESET << std::flush;
        }
        else if (line == "exit") {
            is_running = false;
            break;
        }
        else {
            //unkown command
            save_cursor();
            clear_line(HELP_ROW);
            gotoxy(1, HELP_ROW);
            std::cout << Colors::RED << "Unknown command: " << line << Colors::RESET << std::flush;
            restore_cursor();
        }
    }

    // shutdown
    if (marquee_thread.joinable()) marquee_thread.join();
    clear_screen();
    std::cout << Colors::BRIGHT_RED << "CSOPESY Marquee System shutting down..." << Colors::RESET << "\n";
    std::cout << Colors::BRIGHT_YELLOW << "Thank you for using our system!" << Colors::RESET << "\n";
    return 0;
}
