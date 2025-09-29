#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <iomanip>
#include <queue>
#include <condition_variable>


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

// --- Shared State and Thread Control ---
// Global flag to signal all threads to exit.
std::atomic<bool> is_running{true};

// The command interpreter and display thread share this variable.
std::string prompt_display_buffer = ">> ";
std::mutex prompt_mutex;

// Shared state for the keyboard handler and command interpreter.
std::queue<std::string> command_queue;
std::mutex command_queue_mutex;

// The marquee logic thread and display thread share this variable.
std::string marquee_display_buffer = "";
std::mutex marquee_to_display_mutex;

// Marquee state variables
std::atomic<bool> marquee_running{false};
std::string marquee_text = "Welcome to CSOPESY Marquee!";
std::atomic<int> marquee_speed{200}; // milliseconds
std::atomic<int> marquee_position{0};
std::mutex marquee_state_mutex;

// Condition variable to notify command interpreter of new commands.
std::condition_variable command_queue_cv;

// Current input line buffer
std::string current_input_line;
std::mutex input_line_mutex;

// Flag to indicate if command list should be shown
std::atomic<bool> show_command_list{ false };

// --- Utility Function ---
// Moves the cursor to a specific (x, y) coordinate on the console.
void gotoxy(int x, int y) {
    std::cout << "\033[" << y << ";" << x << "H";
}

// Clear screen function
void clear_screen() {
    std::cout << "\033[2J\033[H";
}

// --- Thread Functions ---
void keyboard_handler_thread_func() {

    std::string command_line;

    while (is_running) {
        if (std::getline(std::cin, command_line)) {
            // Trim whitespace
            command_line.erase(0, command_line.find_first_not_of(" \t\r\n"));
            command_line.erase(command_line.find_last_not_of(" \t\r\n") + 1);
            
            if (!command_line.empty()) {
                {
                    std::lock_guard<std::mutex> lock(command_queue_mutex);
                    command_queue.push(command_line);
                    command_queue_cv.notify_one();
                }
                
                if (command_line == "exit") {
                    is_running = false;
                    break;
                }
            }
        } else {
            // Handle EOF or input stream error
            is_running = false;
            break;
        }
    }
}

void marquee_logic_thread_func(int display_width) {

    while (is_running) {
        if (marquee_running.load()) {
            std::string current_text;
            {
                std::lock_guard<std::mutex> lock(marquee_state_mutex);
                current_text = marquee_text;
            }

            if (!current_text.empty() && display_width > 0) {
                std::string wrapped_text = current_text + std::string(display_width, ' ');

                int wrapped_len = static_cast<int>(wrapped_text.length());
                int pos = marquee_position.load();

                if (wrapped_len == 0) {
                    wrapped_text = current_text;
                    wrapped_len = static_cast<int>(wrapped_text.length());
                }

                if (pos >= wrapped_len) {
                    pos = 0;
                    marquee_position.store(0);
                }

                std::string visible_text;

                if (display_width <= wrapped_len) {
                    int end = pos + display_width;

                    if (end <= wrapped_len) {
                        visible_text = wrapped_text.substr(pos, display_width);
                    } else {
                        int first_len = wrapped_len - pos;
                        int remaining = display_width - first_len;

                        visible_text = wrapped_text.substr(pos, first_len);
                        visible_text += wrapped_text.substr(0, remaining);
                    }
                } else {
                    visible_text.reserve(display_width);

                    int remaining = display_width;
                    int cursor = pos % wrapped_len;

                    while (remaining > 0) {
                        int chunk = std::min(remaining, wrapped_len - cursor);

                        visible_text += wrapped_text.substr(cursor, chunk);
                        remaining -= chunk;
                        cursor = (cursor + chunk) % wrapped_len;
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(marquee_to_display_mutex);
                    marquee_display_buffer = visible_text;
                }

                marquee_position.store((pos + 1) % wrapped_len);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(marquee_speed.load()));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void display_thread_func() {
    const int refresh_rate_ms = 50;
    std::string last_display_buffer;
    std::string last_prompt_buffer;
    std::string last_marquee_text;
    int last_speed = -1;
    bool last_running = false;

    while (is_running) {
        // Only clear and redraw if something changed
        bool need_redraw = false;

        // Check marquee display buffer
        {
            std::lock_guard<std::mutex> lock(marquee_to_display_mutex);
            if (marquee_display_buffer != last_display_buffer) {
                last_display_buffer = marquee_display_buffer;
                need_redraw = true;
            }
        }

        // Check prompt buffer
        {
            std::lock_guard<std::mutex> lock(prompt_mutex);
            if (prompt_display_buffer != last_prompt_buffer) {
                last_prompt_buffer = prompt_display_buffer;
                need_redraw = true;
            }
        }

        // Check marquee text and speed
        {
            std::lock_guard<std::mutex> lock(marquee_state_mutex);
            if (marquee_text != last_marquee_text || marquee_speed.load() != last_speed) {
                last_marquee_text = marquee_text;
                last_speed = marquee_speed.load();
                need_redraw = true;
            }
        }

        // Check running state
        if (marquee_running.load() != last_running) {
            last_running = marquee_running.load();
            need_redraw = true;
        }

        if (need_redraw) {
            clear_screen();

            // Display title
            std::cout << Colors::BOLD << Colors::BRIGHT_BLUE
                      << "========= CSOPESY Marquee Console ========="
                      << Colors::RESET << "\n\n";

            // Display marquee
            // std::cout << Colors::BRIGHT_CYAN << "Marquee Display:" << Colors::RESET << "\n";
            std::cout << Colors::MAGENTA << "+";
            for (int i = 0; i < 41; i++) std::cout << "-";
            std::cout << "+" << Colors::RESET << "\n" << Colors::MAGENTA << "|" << Colors::RESET;

            {
                std::lock_guard<std::mutex> lock(marquee_to_display_mutex);
                if (marquee_display_buffer.length() >= 41) {
                    std::cout << marquee_display_buffer.substr(0, 41);
                } else {
                    std::cout << marquee_display_buffer;
                    for (int i = marquee_display_buffer.length(); i < 41; i++) {
                        std::cout << " ";
                    }
                }
            }

            std::cout << Colors::MAGENTA << "|" << Colors::RESET << "\n" << Colors::MAGENTA << "+";
            for (int i = 0; i < 41; i++) std::cout << "-";
            std::cout << "+" << Colors::RESET << "\n\n";

            // Display status
            std::cout << Colors::BRIGHT_WHITE << "Status: " << Colors::RESET;
            if (marquee_running) {
                std::cout << Colors::BRIGHT_GREEN << "Running" << Colors::RESET;
            } else {
                std::cout << Colors::RED << "Stopped" << Colors::RESET;
            }

            std::cout << Colors::BRIGHT_WHITE << " | Speed: " << Colors::YELLOW
                      << marquee_speed.load() << "ms" << Colors::RESET;

            std::cout << Colors::BRIGHT_WHITE << " | Text: " << Colors::RESET;
            {
                std::lock_guard<std::mutex> lock(marquee_state_mutex);
                std::cout << "\"" << Colors::BRIGHT_YELLOW << marquee_text
                          << Colors::RESET << "\"\n\n";
            }

            // Display commands
            if (show_command_list) {
                std::cout << Colors::BOLD << Colors::BRIGHT_CYAN << "Available Commands:" << Colors::RESET << "\n";
                std::cout << Colors::BRIGHT_YELLOW << "  help" << Colors::WHITE << " - displays the commands and its description\n";
                std::cout << Colors::BRIGHT_YELLOW << "  start_marquee" << Colors::WHITE << " - starts the marquee animation\n";
                std::cout << Colors::BRIGHT_YELLOW << "  stop_marquee" << Colors::WHITE << " - stops the marquee animation\n";
                std::cout << Colors::BRIGHT_YELLOW << "  set_text" << Colors::WHITE << " - accepts a text input and displays it as a marquee\n";
                std::cout << Colors::BRIGHT_YELLOW << "  set_speed" << Colors::WHITE << " - sets the marquee animation refresh in milliseconds\n";
                std::cout << Colors::BRIGHT_RED << "  exit" << Colors::WHITE << " - terminates the console\n\n" << Colors::RESET;
            }

            // Display prompt
            {
                std::lock_guard<std::mutex> lock(prompt_mutex);
                if (prompt_display_buffer.empty() || prompt_display_buffer == ">> ") {
                    std::cout << Colors::CYAN << ">> " << Colors::WHITE;
                } else {
                    std::cout << Colors::CYAN << prompt_display_buffer << Colors::RESET;
                }
            }

            std::cout.flush();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(refresh_rate_ms));
    }
}

// Command processing functions
void show_help() {
    std::lock_guard<std::mutex> lock(prompt_mutex);
    prompt_display_buffer = ">> " + Colors::BRIGHT_GREEN + "Help displayed above. Enter command: " + Colors::WHITE;
}

void start_marquee() {
    marquee_running = true;
    std::lock_guard<std::mutex> lock(prompt_mutex);
    prompt_display_buffer = ">> " + Colors::BRIGHT_GREEN + "Marquee started! Enter command: " + Colors::WHITE;
}

void stop_marquee() {
    marquee_running = false;
    std::lock_guard<std::mutex> lock(prompt_mutex);
    prompt_display_buffer = ">> " + Colors::BRIGHT_YELLOW + "Marquee stopped. Enter command: " + Colors::WHITE;
}

void set_marquee_text() {
    std::lock_guard<std::mutex> lock(prompt_mutex);
    prompt_display_buffer = ">> " + Colors::CYAN + "Enter text for marquee: " + Colors::WHITE;
}

void set_marquee_speed() {
    std::lock_guard<std::mutex> lock(prompt_mutex);
    prompt_display_buffer = ">> " + Colors::CYAN + "Enter speed in milliseconds: " + Colors::WHITE;
}

// --- Main Function (Command Interpreter Thread) ---
int main() {
    // Enable ANSI colors on Windows
    #ifdef _WIN32
    system("chcp 65001 > nul");
    //  system("");
    #endif
    
    // Start the three worker threads.
    std::thread marquee_logic_thread(marquee_logic_thread_func, 41);
    std::thread display_thread(display_thread_func);
    std::thread keyboard_handler_thread(keyboard_handler_thread_func);

    // State for multi-step commands
    enum CommandState { NORMAL, WAITING_TEXT, WAITING_SPEED };
    CommandState current_state = NORMAL;

    // Main loop that processes commands from the queue.
    while (is_running) {
        std::string command_line;
        {
            std::unique_lock<std::mutex> lock(command_queue_mutex);
            command_queue_cv.wait(lock, [] { return !command_queue.empty() || !is_running; });
            if (!command_queue.empty()) {
                command_line = command_queue.front();
                command_queue.pop();
            }
        }
        
        if (!command_line.empty()) {
            // Process commands based on current state
            if (current_state == WAITING_TEXT) {
                {
                    std::lock_guard<std::mutex> lock(marquee_state_mutex);
                    marquee_text = command_line;
                    marquee_position = 0; // Reset position for new text
                }
                {
                    std::lock_guard<std::mutex> lock(prompt_mutex);
                    prompt_display_buffer = ">> " + Colors::GREEN + "Text set to \"" + command_line + "\". Enter command: " + Colors::WHITE;
                }

                show_command_list = false;
                current_state = NORMAL;
            }
            else if (current_state == WAITING_SPEED) {
                try {
                    int speed = std::stoi(command_line);
                    if (speed > 0) {
                        marquee_speed = speed;
                        std::lock_guard<std::mutex> lock(prompt_mutex);
                        prompt_display_buffer = ">> " + Colors::GREEN + "Speed set to " + command_line + "ms. Enter command: " + Colors::WHITE;
                    } else {
                        std::lock_guard<std::mutex> lock(prompt_mutex);
                        prompt_display_buffer = ">> " + Colors::RED + "Invalid speed. Enter a positive number: " + Colors::WHITE;
                        continue; // Stay in WAITING_SPEED state
                    }
                } catch (const std::exception&) {
                    std::lock_guard<std::mutex> lock(prompt_mutex);
                    prompt_display_buffer = ">> " + Colors::RED + "Invalid speed. Enter a positive number: " + Colors::WHITE;
                    continue; // Stay in WAITING_SPEED state
                }

                show_command_list = false;
                current_state = NORMAL;
            }
            else {
                // Normal command processing
                if (command_line == "help") {
                    show_help();
                    show_command_list = true;
                }
                else if (command_line == "start_marquee") {
                    start_marquee();
                    show_command_list = false;
                }
                else if (command_line == "stop_marquee") {
                    stop_marquee();
                    show_command_list = false;
                }
                else if (command_line == "set_text") {
                    set_marquee_text();
                    show_command_list = false;
                    current_state = WAITING_TEXT;
                }
                else if (command_line == "set_speed") {
                    set_marquee_speed();
                    show_command_list = false;
                    current_state = WAITING_SPEED;
                }
                else if (command_line == "exit") {
                    is_running = false;
                    show_command_list = false;
                }
                else {
                    std::lock_guard<std::mutex> lock(prompt_mutex);
                    prompt_display_buffer = ">> " + Colors::RED + "Unknown command '" + command_line + "'. Type 'help' for available commands: " + Colors::WHITE;
                    show_command_list = false;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Shutdown message
    clear_screen();
    std::cout << Colors::BRIGHT_RED << "CSOPESY Marquee System shutting down..." << Colors::RESET << "\n";
    std::cout << Colors::BRIGHT_YELLOW << "Thank you for using our system!" << Colors::RESET << "\n";

    // Join threads to ensure they finish cleanly.
    if (marquee_logic_thread.joinable()) {
        marquee_logic_thread.join();
    }
    if (display_thread.joinable()) {
        display_thread.join();
    }
    if (keyboard_handler_thread.joinable()) {
        keyboard_handler_thread.join();
    }

    return 0;
}