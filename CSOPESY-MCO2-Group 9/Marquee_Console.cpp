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
std::atomic<bool> is_running{ true };

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
std::atomic<bool> marquee_running{ false };
std::string marquee_text = "Welcome to CSOPESY Marquee!";
std::atomic<int> marquee_speed{ 200 }; // milliseconds
std::atomic<int> marquee_position{ 0 };
std::mutex marquee_state_mutex;

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
            // Trim whitespace - safer version to prevent out-of-bounds
            if (!command_line.empty()) {
                // Trim leading whitespace
                size_t start = command_line.find_first_not_of(" \t\r\n");
                if (start == std::string::npos) {
                    command_line = ""; // String is all whitespace
                } else {
                    // Trim trailing whitespace
                    size_t end = command_line.find_last_not_of(" \t\r\n");
                    command_line = command_line.substr(start, end - start + 1);
                }
            }

            if (!command_line.empty()) {
                {
                    std::lock_guard<std::mutex> lock(command_queue_mutex);
                    command_queue.push(command_line);
                }

                if (command_line == "exit") {
                    is_running = false;
                    break;
                }
            }
        }
        else {
            // Handle EOF or input stream error
            is_running = false;
            break;
        }
    }
}

void marquee_logic_thread_func(int display_width) {
    while (is_running) {
        if (marquee_running) {
            std::string current_text;
            {
                std::lock_guard<std::mutex> lock(marquee_state_mutex);
                current_text = marquee_text;
            }

            if (!current_text.empty()) {
                int text_length = static_cast<int>(current_text.length());
                int pos = marquee_position.load();

                std::string display_line;
                
                if (text_length <= display_width) {
                    // Text fits in display - simple left-to-right scrolling
                    int total_cycle = display_width + text_length;
                    if (pos >= total_cycle) {
                        marquee_position = 0;
                        pos = 0;
                    }

                    display_line = std::string(display_width, ' ');
                    
                    if (pos < display_width) {
                        // Text coming in from right
                        int start_display_pos = display_width - pos - 1;
                        int chars_to_show = std::min(text_length, pos + 1);
                        if (start_display_pos >= 0 && chars_to_show > 0 && start_display_pos + chars_to_show <= display_width) {
                            display_line.replace(start_display_pos, chars_to_show, 
                                current_text.substr(0, chars_to_show));
                        }
                    } else {
                        // Text scrolling off to left
                        int chars_already_gone = pos - display_width + 1;
                        int chars_remaining = text_length - chars_already_gone;
                        if (chars_remaining > 0 && chars_already_gone < text_length) {
                            int chars_to_show = std::min(chars_remaining, display_width);
                            if (chars_to_show > 0) {
                                display_line.replace(0, chars_to_show,
                                    current_text.substr(chars_already_gone, chars_to_show));
                            }
                        }
                    }
                }
                else {
                    // Text is longer than display - continuous scrolling
                    if (pos >= text_length) {
                        marquee_position = 0;
                        pos = 0;
                    }
                    
                    display_line = current_text.substr(pos, display_width);
                    // If we don't have enough characters, wrap around
                    if (static_cast<int>(display_line.length()) < display_width) {
                        int remaining = display_width - static_cast<int>(display_line.length());
                        display_line += " " + current_text.substr(0, std::min(remaining - 1, text_length));
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(marquee_to_display_mutex);
                    marquee_display_buffer = display_line; // Store plain text without colors
                }

                marquee_position++;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(marquee_speed.load()));
    }
}

void display_thread_func() {
    const int refresh_rate_ms = 50;
    const int marquee_display_width = 40;
    
    while (is_running) {
        clear_screen();

        // Display title
        std::cout << Colors::BOLD << Colors::BRIGHT_BLUE
            << "=== CSOPESY Marquee Console ==="
            << Colors::RESET << "\n\n";

        // Display marquee
        std::cout << Colors::BRIGHT_CYAN << "Marquee Display:" << Colors::RESET << "\n";
        std::cout << Colors::MAGENTA << "+";
        for (int i = 0; i < marquee_display_width; i++) std::cout << "-";
        std::cout << "+" << Colors::RESET << "\n" << Colors::MAGENTA << "|" << Colors::RESET;

        {
            std::lock_guard<std::mutex> lock(marquee_to_display_mutex);
            std::string display_content = marquee_display_buffer;
            
            // Ensure we display exactly marquee_display_width characters
            if (static_cast<int>(display_content.length()) > marquee_display_width) {
                display_content = display_content.substr(0, marquee_display_width);
            }
            
            // Add color to the display content
            std::cout << Colors::BRIGHT_YELLOW << display_content << Colors::RESET;
            
            // Pad with spaces if needed
            int padding = marquee_display_width - static_cast<int>(display_content.length());
            for (int i = 0; i < padding; i++) {
                std::cout << " ";
            }
        }

        std::cout << Colors::MAGENTA << "|" << Colors::RESET << "\n" << Colors::MAGENTA << "+";
        for (int i = 0; i < marquee_display_width; i++) std::cout << "-";
        std::cout << "+" << Colors::RESET << "\n\n";

        // Display status
        std::cout << Colors::BRIGHT_WHITE << "Status: " << Colors::RESET;
        if (marquee_running) {
            std::cout << Colors::BRIGHT_GREEN << "Running" << Colors::RESET;
        }
        else {
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
        std::cout << Colors::BOLD << Colors::BRIGHT_CYAN << "Available Commands:" << Colors::RESET << "\n";
        std::cout << Colors::BRIGHT_YELLOW << "  help" << Colors::WHITE << " - Show this help\n";
        std::cout << Colors::BRIGHT_YELLOW << "  start_marquee" << Colors::WHITE << " - Start the marquee animation\n";
        std::cout << Colors::BRIGHT_YELLOW << "  stop_marquee" << Colors::WHITE << " - Stop the marquee animation\n";
        std::cout << Colors::BRIGHT_YELLOW << "  set_text" << Colors::WHITE << " - Set marquee text\n";
        std::cout << Colors::BRIGHT_YELLOW << "  set_speed" << Colors::WHITE << " - Set animation speed (ms)\n";
        std::cout << Colors::BRIGHT_RED << "  exit" << Colors::WHITE << " - Exit the program\n\n" << Colors::RESET;

        // Display prompt
        {
            std::lock_guard<std::mutex> lock(prompt_mutex);
            if (prompt_display_buffer.empty() || prompt_display_buffer == ">> ") {
                std::cout << Colors::CYAN << ">> " << Colors::WHITE;
            }
            else {
                std::cout << Colors::CYAN << prompt_display_buffer << Colors::RESET;
            }
        }

        std::cout.flush();
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
    system("");
#endif

    // Start the three worker threads.
    std::thread marquee_logic_thread(marquee_logic_thread_func, 40);
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
                current_state = NORMAL;
            }
            else if (current_state == WAITING_SPEED) {
                try {
                    int speed = std::stoi(command_line);
                    if (speed > 0) {
                        marquee_speed = speed;
                        std::lock_guard<std::mutex> lock(prompt_mutex);
                        prompt_display_buffer = ">> " + Colors::GREEN + "Speed set to " + command_line + "ms. Enter command: " + Colors::WHITE;
                    }
                    else {
                        std::lock_guard<std::mutex> lock(prompt_mutex);
                        prompt_display_buffer = ">> " + Colors::RED + "Invalid speed. Enter a positive number: " + Colors::WHITE;
                        continue; // Stay in WAITING_SPEED state
                    }
                }
                catch (const std::exception&) {
                    std::lock_guard<std::mutex> lock(prompt_mutex);
                    prompt_display_buffer = ">> " + Colors::RED + "Invalid speed. Enter a positive number: " + Colors::WHITE;
                    continue; // Stay in WAITING_SPEED state
                }
                current_state = NORMAL;
            }
            else {
                // Normal command processing
                if (command_line == "help") {
                    show_help();
                }
                else if (command_line == "start_marquee") {
                    start_marquee();
                }
                else if (command_line == "stop_marquee") {
                    stop_marquee();
                }
                else if (command_line == "set_text") {
                    set_marquee_text();
                    current_state = WAITING_TEXT;
                }
                else if (command_line == "set_speed") {
                    set_marquee_speed();
                    current_state = WAITING_SPEED;
                }
                else if (command_line == "exit") {
                    is_running = false;
                }
                else {
                    std::lock_guard<std::mutex> lock(prompt_mutex);
                    prompt_display_buffer = ">> " + Colors::RED + "Unknown command '" + command_line + "'. Type 'help' for available commands: " + Colors::WHITE;
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