/*
    Course & Section: CSOPESY | S13
    Assessment: MCO2
    Group 9 Developers: Alvarez, Ivan Antonio T.
                        Barlaan, Bahir Benjamin C.
                        Co, Joshua Benedict B.
                        Tan, Reyvin Matthew T.
    Version Date: October 1, 2025
*/

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <queue>
#include <condition_variable>

#ifdef _WIN32
#include <windows.h>
#endif

// --- Color Codes ---
// Part of: Console UI Implementation
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
// Part of: Console UI Implementation
struct ConsoleLayout {
    int screen_width = 120;       // terminal width
    int screen_height = 30;       // terminal height
    int marquee_width = 41;       // inner width of marquee box
    int marquee_top_row = 2;      // box top border
    int marquee_text_row = 3;     // inner text row (where scrolling appears)
    int marquee_bottom_row = 4;   // box bottom border
    int status_row = 6;           // status shows here
    int help_row = 7;             // help messages show here
    int prompt_row = 22;          // input prompt locked here (adjusted for developer info)
    int prompt_col = 1;           // column where prompt starts (">> ")
};

// --- Shared state ---
std::atomic<bool> is_running{ true };
std::atomic<bool> marquee_running{ false };
std::string marquee_text = " Welcome to CSOPESY Marquee! ";
std::atomic<int> marquee_speed{ 200 };  // ms
std::atomic<int> marquee_position{ 0 };
std::mutex marquee_state_mutex;
ConsoleLayout layout;
std::mutex layout_mutex;
std::mutex console_mutex;
std::mutex prompt_mutex;
std::string prompt_display = ">> ";
std::atomic<bool> help_visible{ false };

// --- Keyboard Handler ---
std::queue<std::string> command_queue;
std::mutex command_queue_mutex;
std::condition_variable command_queue_cv;

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

// --- Get current console size ---
void get_console_size(int& width, int& height) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
    else {
        width = 120;
        height = 30;
    }
#else
    width = 120;
    height = 30;
#endif
}

// --- Update layout based on current console size ---
// Part of: Console UI Implementation
void update_layout() {
    std::lock_guard<std::mutex> lock(layout_mutex);
    get_console_size(layout.screen_width, layout.screen_height);

    // adjust ONLY marquee width based on screen width (but keep reasonable limits)
    layout.marquee_width = (std::min)((std::max)(41, layout.screen_width - 20), layout.screen_width - 4);

    // keep ALL elements FIXED - wag palitan based on screen size
    layout.marquee_top_row = 2;                        // FIXED
    layout.marquee_text_row = 3;                       // FIXED  
    layout.marquee_bottom_row = 4;                     // FIXED
    layout.status_row = 6;                             // FIXED
    layout.help_row = 7;                               // FIXED
    layout.prompt_row = layout.help_row + 15;          // FIXED 
    layout.prompt_col = 1;                             // FIXED
}

// --- 1-based column/row ---
// Part of: Console UI Implementation
void gotoxy(int col, int row) {
    std::lock_guard<std::mutex> lock(console_mutex);
    if (row < 1) row = 1;
    if (col < 1) col = 1;
    printf("\033[%d;%dH", row, col);
    fflush(stdout);
}

// --- Save cursor position ---
// Part of: Console UI Implementation
void save_cursor() {
    std::lock_guard<std::mutex> lock(console_mutex);
    printf("\033[s");
    fflush(stdout);
}

// --- Restore cursor position ---
// Part of: Console UI Implementation
void restore_cursor() {
    std::lock_guard<std::mutex> lock(console_mutex);
    printf("\033[u");
    fflush(stdout);
}

// --- Clear screen ---
// Part of: Console UI Implementation
void clear_screen() {
    std::lock_guard<std::mutex> lock(console_mutex);
    printf("\033[2J\033[H");
    fflush(stdout);
}

// --- Clear a specific line ---
// Part of: Console UI Implementation
void clear_line(int row, int width = 0) {
    if (width == 0) {
        std::lock_guard<std::mutex> lock(layout_mutex);
        width = layout.screen_width;
    }
    gotoxy(1, row);
    std::cout << std::string(width, ' ') << std::flush;
}

// --- Displays help line ---
// Part of: Display Implementation
void show_help_line() {
    std::lock_guard<std::mutex> lock(layout_mutex);
    save_cursor();

    // clear enough lines for multi-line help (ensure we don't clear the prompt area)
    int max_help_row = layout.prompt_row - 2;
    int end_help_row = (std::min)(layout.help_row + 13, max_help_row);
    for (int row = layout.help_row + 8; row <= end_help_row + 2; ++row) {
        clear_line(row, layout.screen_width);
    }

    // add a blank line before help list
    gotoxy(1, layout.help_row + 8);
    std::cout << std::endl;

    // print help starting at adjusted HELP_ROW
    gotoxy(1, layout.help_row + 9);
    std::cout << Colors::BOLD << Colors::BRIGHT_CYAN << "Available Commands:" << Colors::RESET;
    gotoxy(1, layout.help_row + 10);
    std::cout << Colors::BRIGHT_YELLOW << "  help" << Colors::WHITE << " - displays the commands and its description";
    gotoxy(1, layout.help_row + 11);
    std::cout << Colors::BRIGHT_YELLOW << "  start_marquee" << Colors::WHITE << " - starts the marquee animation";
    gotoxy(1, layout.help_row + 12);
    std::cout << Colors::BRIGHT_YELLOW << "  stop_marquee" << Colors::WHITE << " - stops the marquee animation";
    gotoxy(1, layout.help_row + 13);
    std::cout << Colors::BRIGHT_YELLOW << "  set_text" << Colors::WHITE << " - accepts a text input and displays it as a marquee";
    gotoxy(1, layout.help_row + 14);
    std::cout << Colors::BRIGHT_YELLOW << "  set_speed" << Colors::WHITE << " - sets the marquee animation refresh in milliseconds";
    gotoxy(1, layout.help_row + 15);
    std::cout << Colors::BRIGHT_RED << "  exit" << Colors::WHITE << " - terminates the console";
    std::cout << Colors::RESET << std::flush;

    restore_cursor();
}

// --- Draw the static UI once (title, marquee box, initial status/help, prompt) ---
// Part of: Display Implementation
void display_static_ui() {
    update_layout(); // updates the layout based on current console size

    clear_screen();

    // title (row 1)
    gotoxy(1, 1);
    std::cout << Colors::BOLD << Colors::BRIGHT_BLUE
        << "========= Welcome to CSOPESY Marquee Console ========="
        << Colors::RESET;

    // add a blank line before group developer list
    gotoxy(1, 2);
    std::cout << std::endl;

    // developer info (rows 3-6)
    gotoxy(1, 3);
    std::cout << Colors::BRIGHT_CYAN << "Group Developer: Alvarez, Ivan Antonio T." << Colors::RESET;
    gotoxy(1, 4);
    std::cout << Colors::BRIGHT_CYAN << "                 Barlaan, Bahir Benjamin C." << Colors::RESET;
    gotoxy(1, 5);
    std::cout << Colors::BRIGHT_CYAN << "                 Co, Joshua Benedict B." << Colors::RESET;
    gotoxy(1, 6);
    std::cout << Colors::BRIGHT_CYAN << "                 Tan, Reyvin Matthew T." << Colors::RESET;

    // version date (row 7)
    gotoxy(1, 7);
    std::cout << std::endl;
    gotoxy(1, 8);
    std::cout << Colors::BRIGHT_YELLOW << "Version date: October 1, 2025" << Colors::RESET;

    // marquee box (shifted down by 2 rows)
    gotoxy(1, layout.marquee_top_row + 8);
    std::cout << Colors::MAGENTA << "+" << std::string(layout.marquee_width, '-') << "+" << Colors::RESET;
    gotoxy(1, layout.marquee_text_row + 8);
    std::cout << Colors::MAGENTA << "|" << Colors::RESET << std::string(layout.marquee_width, ' ') << Colors::MAGENTA << "|" << Colors::RESET;
    gotoxy(1, layout.marquee_bottom_row + 8);
    std::cout << Colors::MAGENTA << "+" << std::string(layout.marquee_width, '-') << "+" << Colors::RESET;

    // status (shifted down by 2 rows)
    gotoxy(1, layout.status_row + 8);
    std::cout << Colors::BRIGHT_WHITE << "Status: " << Colors::RESET;
    if (marquee_running.load())
        std::cout << Colors::BRIGHT_GREEN << "Running" << Colors::RESET;
    else
        std::cout << Colors::RED << "Stopped" << Colors::RESET;
    std::cout << Colors::BRIGHT_WHITE << " | Speed: " << Colors::YELLOW
        << marquee_speed.load() << "ms" << Colors::RESET;

    // add a blank line before help tip (shifted down by 2 rows)
    gotoxy(1, layout.help_row + 8);
    std::cout << std::endl;
    gotoxy(1, layout.help_row + 9);
    std::cout << Colors::BRIGHT_GREEN << "Type 'help' for available commands." << Colors::RESET;

    if (help_visible.load()) {
        show_help_line();
    }

    // prompt (shifted down by 2 rows)
    gotoxy(layout.prompt_col, layout.prompt_row + 2);
    std::cout << Colors::CYAN << prompt_display << Colors::RESET << std::flush;
}

// --- Update status line ---
// Part of: Display Implementation
void update_status_line() {
    std::lock_guard<std::mutex> lock(layout_mutex);
    save_cursor();
    clear_line(layout.status_row + 8, layout.screen_width);
    gotoxy(1, layout.status_row + 8);
    std::cout << Colors::BRIGHT_WHITE << "Status: " << Colors::RESET;
    if (marquee_running.load())
        std::cout << Colors::BRIGHT_GREEN << "Running" << Colors::RESET;
    else
        std::cout << Colors::RED << "Stopped" << Colors::RESET;
    std::cout << Colors::BRIGHT_WHITE << " | Speed: " << Colors::YELLOW
        << marquee_speed.load() << "ms" << Colors::RESET << std::flush;
    restore_cursor();
}

// --- Marquee thread: updates ONLY the MARQUEE_TEXT_ROW ---
// Part of: Marquee Animation Logic
void marquee_thread_func(int display_width) {
    while (is_running) {
        if (marquee_running.load()) {
            std::string text_copy;
            int current_width;
            {
                std::lock_guard<std::mutex> lock(marquee_state_mutex);
                text_copy = marquee_text;
            }
            {
                std::lock_guard<std::mutex> lock(layout_mutex);
                current_width = layout.marquee_width;
            }

            if (!text_copy.empty() && current_width > 0) {
                std::string buffer = text_copy + std::string(current_width, ' ');
                int len = static_cast<int>(buffer.size());
                int pos = marquee_position.load();
                if (pos >= len) pos = 0;

                std::string visible;
                if (pos + current_width <= len) {
                    visible = buffer.substr(pos, current_width);
                }
                else {
                    int first = len - pos;
                    visible = buffer.substr(pos, first) + buffer.substr(0, current_width - first);
                }

                save_cursor();
                {
                    std::lock_guard<std::mutex> layout_lock(layout_mutex);
                    gotoxy(2, layout.marquee_text_row + 8);
                }

                {
                    std::lock_guard<std::mutex> lock(console_mutex);
                    printf("%s%-*s%s", Colors::WHITE.c_str(), current_width, visible.c_str(), Colors::RESET.c_str());
                    fflush(stdout);
                }

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

// --- Set marquee text ---
// Part of: Marquee Animation Logic
void set_marquee_text(const std::string& text) {
    std::lock_guard<std::mutex> lock(marquee_state_mutex);
    marquee_text = text.empty() ? " " : text;
    marquee_position.store(0);
}

// --- Set marquee speed ---
// Part of: Marquee Animation Logic
void set_marquee_speed(int spd) {
    if (spd > 0) marquee_speed.store(spd);
    update_status_line();
}

// --- Check if console size changed and redraw if needed ---
void check_and_handle_resize() {
    int current_width, current_height;
    get_console_size(current_width, current_height);

    bool size_changed = false;
    {
        std::lock_guard<std::mutex> lock(layout_mutex);
        if (current_width != layout.screen_width || current_height != layout.screen_height) {
            size_changed = true;
        }
    }

    if (size_changed) {
        // small delay to let resize complete and prevent flicker
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        display_static_ui(); // This will update layout and redraw
    }
}

// --- Background resize monitoring thread ---
void resize_monitor_thread_func() {
    while (is_running) {
        check_and_handle_resize();
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // check every 100ms
    }
}

// --- Keyboard Handler ---
void keyboard_handler_thread_func() {
    std::string line;
    while (is_running) {
        if (!std::getline(std::cin, line)) { // EOF or error
            is_running = false;
            command_queue_cv.notify_all();
            break;
        }

        // sanitize input
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            if (!s.empty()) s.erase(s.find_last_not_of(" \t\r\n") + 1);
            };
        trim(line);

        if (line.empty()) continue;

        {
            std::lock_guard<std::mutex> lock(command_queue_mutex);
            command_queue.push(line);
        }
        command_queue_cv.notify_one();
    }
}

// --- Helper: redraw prompt and position cursor ---
void redraw_prompt_and_place_cursor() {
    std::lock_guard<std::mutex> prompt_lock(prompt_mutex);
    std::lock_guard<std::mutex> layout_lock(layout_mutex);
    clear_line(layout.prompt_row + 2, layout.screen_width);
    gotoxy(layout.prompt_col, layout.prompt_row + 2);
    std::cout << Colors::CYAN << prompt_display << Colors::RESET << std::flush;
    int input_col = layout.prompt_col + static_cast<int>(prompt_display.size());
    gotoxy(input_col, layout.prompt_row + 2);
}

// --- Main ---
// Part of: Command Recognition & Command Interpreter
int main() {
    enable_ansi_on_windows();

    // draw UI once
    display_static_ui();

    // start marquee thread
    std::thread marquee_thread(marquee_thread_func, layout.marquee_width);

    // start resize monitoring thread
    std::thread resize_thread(resize_monitor_thread_func);

    // --- Keyboard Handler ---
    std::thread keyboard_thread(keyboard_handler_thread_func);

    enum CommandState { NORMAL, WAITING_TEXT, WAITING_SPEED };
    CommandState state = NORMAL;

    while (is_running) {
        std::string line;
        {
            std::unique_lock<std::mutex> lock(command_queue_mutex);
            command_queue_cv.wait(lock, [] {
                return !command_queue.empty() || !is_running.load();
                });

            if (!is_running) break;

            line = command_queue.front();
            command_queue.pop();
        }

        
        // cursor to the prompt position.
        redraw_prompt_and_place_cursor();

        // handle WAITING states: these are inputs for multi-step commands
        if (state == WAITING_TEXT) {
            set_marquee_text(line);
            state = NORMAL;
            {
                std::lock_guard<std::mutex> lock(prompt_mutex);
                prompt_display = ">> ";
            }
            update_status_line();

            // redraw prompt for next input
            redraw_prompt_and_place_cursor();
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

            // redraw prompt for next input
            redraw_prompt_and_place_cursor();
            continue;
        }

        // normal commands
        if (line == "help") {
            help_visible = true;
            // ensure prompt is visible and saved for restore
            redraw_prompt_and_place_cursor();
            show_help_line();
        }
        else if (line == "start_marquee") {
            marquee_running = true;
            help_visible = false;

            {
                std::lock_guard<std::mutex> layout_lock(layout_mutex);
                // clear help area
                for (int row = layout.help_row + 8; row <= layout.help_row + 15; ++row)
                    clear_line(row, layout.screen_width);
                // print help tip
                gotoxy(1, layout.help_row + 8);
                std::cout << std::endl;
                gotoxy(1, layout.help_row + 9);
                std::cout << Colors::BRIGHT_GREEN << "Type 'help' for available commands." << Colors::RESET << std::flush;
            }
            update_status_line();
        }
        else if (line == "stop_marquee") {
            marquee_running = false;
            help_visible = false;

            {
                std::lock_guard<std::mutex> layout_lock(layout_mutex);
                // clear help area
                for (int row = layout.help_row + 8; row <= layout.help_row + 15; ++row)
                    clear_line(row, layout.screen_width);
                // print help tip
                gotoxy(1, layout.help_row + 8);
                std::cout << std::endl;
                gotoxy(1, layout.help_row + 9);
                std::cout << Colors::BRIGHT_GREEN << "Type 'help' for available commands." << Colors::RESET << std::flush;
            }
            update_status_line();
        }
        else if (line == "set_text") {
            help_visible = false;

            state = WAITING_TEXT;
            {
                std::lock_guard<std::mutex> lock(prompt_mutex);
                prompt_display = "Enter text for marquee: ";
            }
            // immediately show updated prompt so user knows to type the text
            redraw_prompt_and_place_cursor();

            {
                std::lock_guard<std::mutex> layout_lock(layout_mutex);
                // clear help area
                for (int row = layout.help_row + 8; row <= layout.help_row + 15; ++row)
                    clear_line(row, layout.screen_width);
                // print help tip
                gotoxy(1, layout.help_row + 8);
                std::cout << std::endl;
                gotoxy(1, layout.help_row + 9);
                std::cout << Colors::BRIGHT_GREEN << "Type 'help' for available commands." << Colors::RESET << std::flush;
            }
            update_status_line();
        }
        else if (line == "set_speed") {
            help_visible = false;

            state = WAITING_SPEED;
            {
                std::lock_guard<std::mutex> lock(prompt_mutex);
                prompt_display = "Enter speed in ms: ";
            }
            // immediately show updated prompt so user knows to type the speed
            redraw_prompt_and_place_cursor();

            {
                std::lock_guard<std::mutex> layout_lock(layout_mutex);
                // clear help area
                for (int row = layout.help_row + 8; row <= layout.help_row + 15; ++row)
                    clear_line(row, layout.screen_width);
                // print help tip
                gotoxy(1, layout.help_row + 8);
                std::cout << std::endl;
                gotoxy(1, layout.help_row + 9);
                std::cout << Colors::BRIGHT_GREEN << "Type 'help' for available commands." << Colors::RESET << std::flush;
            }
            update_status_line();
        }
        else if (line == "exit") {
            is_running = false;
            break;
        }
        else {
            //unknown command
            save_cursor();
            std::lock_guard<std::mutex> layout_lock(layout_mutex);
            clear_line(layout.help_row + 7, layout.screen_width);
            gotoxy(1, layout.help_row + 7);
            std::cout << Colors::RED << "Unknown command: " << line << Colors::RESET << std::flush;
            restore_cursor();
        }

        // redraw prompt for next input (default prompt or state prompt)
        redraw_prompt_and_place_cursor();
    }

    // shutdown
    if (marquee_thread.joinable()) marquee_thread.join();
    if (resize_thread.joinable()) resize_thread.join();
    if (keyboard_thread.joinable()) keyboard_thread.join();
    clear_screen();
    std::cout << Colors::BRIGHT_RED << "CSOPESY Marquee System shutting down..." << Colors::RESET << "\n";
    std::cout << Colors::BRIGHT_YELLOW << "Thank you for using our system!" << Colors::RESET << "\n";
    return 0;
}
