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

// --- Shared State and Thread Control ---
// Global flag to signal all threads to exit.
std::atomic<bool> is_running{true};

// The command interpreter and display thread share this variable.
std::string prompt_display_buffer = ">> ";
std::mutex prompt_mutex;

// Shared state for the keyboard handler and command interpreter.
std::queue<std::string> command_queue;
std::mutex command_queue_mutex;
std::condition_variable command_cv;

// The marquee logic thread and display thread share this variable.
std::string marquee_display_buffer = "";
std::mutex marquee_to_display_mutex;

// Marquee state variables
std::atomic<bool> marquee_running{false};
std::string marquee_text = "Welcome to CSOPESY Marquee!";
std::atomic<int> marquee_speed{200}; // milliseconds
std::atomic<int> marquee_position{0};
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
            // Trim whitespace
            command_line.erase(0, command_line.find_first_not_of(" \t\r\n"));
            command_line.erase(command_line.find_last_not_of(" \t\r\n") + 1);
            
            if (!command_line.empty()) {
                {
                    std::lock_guard<std::mutex> lock(command_queue_mutex);
                    command_queue.push(command_line);
                }
                command_cv.notify_one();
                
                if (command_line == "exit") {
                    is_running = false;
                    break;
                }
            }
        } else {
            // Handle EOF or input stream error
            is_running = false;
            command_cv.notify_all();
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
                int text_length = current_text.length();
                int pos = marquee_position.load();
                
                std::string display_line;
                if (text_length <= display_width) {
                    // Text fits in display, center it or scroll it
                    if (pos >= display_width + text_length) {
                        marquee_position = 0;
                        pos = 0;
                    }
                    
                    if (pos < text_length) {
                        display_line = std::string(display_width, ' ');
                        int start_pos = (pos < display_width) ? display_width - pos - 1 : 0;
                        int chars_to_show = std::min(text_length, display_width - start_pos);
                        if (start_pos >= 0 && chars_to_show > 0) {
                            display_line.replace(start_pos, chars_to_show, current_text.substr(0, chars_to_show));
                        }
                    } else {
                        int offset = pos - text_length;
                        if (offset < display_width) {
                            display_line = std::string(display_width, ' ');
                            int chars_to_show = std::min(text_length, display_width - offset);
                            if (chars_to_show > 0) {
                                display_line.replace(display_width - offset - chars_to_show, chars_to_show, 
                                                   current_text.substr(text_length - chars_to_show, chars_to_show));
                            }
                        } else {
                            display_line = std::string(display_width, ' ');
                        }
                    }
                } else {
                    // Text is longer than display, scroll it
                    if (pos >= text_length) {
                        marquee_position = 0;
                        pos = 0;
                    }
                    display_line = current_text.substr(pos, display_width);
                    if (display_line.length() < display_width) {
                        display_line += " " + current_text.substr(0, display_width - display_line.length());
                    }
                }
                
                {
                    std::lock_guard<std::mutex> lock(marquee_to_display_mutex);
                    marquee_display_buffer = display_line;
                }
                
                marquee_position++;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(marquee_speed.load()));
    }
}

void display_thread_func() {
    const int refresh_rate_ms = 50;
    while (is_running) {
        clear_screen();
        
        // Display title
        std::cout << "=== CSOPESY Marquee Console ===\n\n";
        
        // Display marquee
        std::cout << "Marquee Display:\n";
        std::cout << "+";
        for (int i = 0; i < 40; i++) std::cout << "-";
        std::cout << "+\n|";
        
        {
            std::lock_guard<std::mutex> lock(marquee_to_display_mutex);
            if (marquee_display_buffer.length() >= 40) {
                std::cout << marquee_display_buffer.substr(0, 40);
            } else {
                std::cout << marquee_display_buffer;
                for (int i = marquee_display_buffer.length(); i < 40; i++) {
                    std::cout << " ";
                }
            }
        }
        
        std::cout << "|\n+";
        for (int i = 0; i < 40; i++) std::cout << "-";
        std::cout << "+\n\n";
        
        // Display status
        std::cout << "Status: " << (marquee_running ? "Running" : "Stopped") << "\n";
        std::cout << "Speed: " << marquee_speed.load() << "ms\n";
        std::cout << "Text: ";
        {
            std::lock_guard<std::mutex> lock(marquee_state_mutex);
            std::cout << "\"" << marquee_text << "\"\n\n";
        }
        
        // Display commands
        std::cout << "Available Commands:\n";
        std::cout << "  help - Show this help\n";
        std::cout << "  start_marquee - Start the marquee animation\n";
        std::cout << "  stop_marquee - Stop the marquee animation\n";
        std::cout << "  set_text - Set marquee text\n";
        std::cout << "  set_speed - Set animation speed (ms)\n";
        std::cout << "  exit - Exit the program\n\n";
        
        // Display prompt
        {
            std::lock_guard<std::mutex> lock(prompt_mutex);
            std::cout << prompt_display_buffer;
        }
        
        std::cout.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(refresh_rate_ms));
    }
}

// Command processing functions
void show_help() {
    std::lock_guard<std::mutex> lock(prompt_mutex);
    prompt_display_buffer = ">> Help displayed above. Enter command: ";
}

void start_marquee() {
    marquee_running = true;
    std::lock_guard<std::mutex> lock(prompt_mutex);
    prompt_display_buffer = ">> Marquee started. Enter command: ";
}

void stop_marquee() {
    marquee_running = false;
    std::lock_guard<std::mutex> lock(prompt_mutex);
    prompt_display_buffer = ">> Marquee stopped. Enter command: ";
}

void set_marquee_text() {
    std::lock_guard<std::mutex> lock(prompt_mutex);
    prompt_display_buffer = ">> Enter text for marquee: ";
    // Note: The actual text input will be handled in the next command input
}

void set_marquee_speed() {
    std::lock_guard<std::mutex> lock(prompt_mutex);
    prompt_display_buffer = ">> Enter speed in milliseconds: ";
    // Note: The actual speed input will be handled in the next command input
}

// --- Main Function (Command Interpreter Thread) ---
int main() {
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
            command_cv.wait(lock, [] {
                return !command_queue.empty() || !is_running;
            });
            
            if (!is_running && command_queue.empty()) break;
            
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
                    prompt_display_buffer = ">> Text set to \"" + command_line + "\". Enter command: ";
                }
                current_state = NORMAL;
            }
            else if (current_state == WAITING_SPEED) {
                try {
                    int speed = std::stoi(command_line);
                    if (speed > 0) {
                        marquee_speed = speed;
                        std::lock_guard<std::mutex> lock(prompt_mutex);
                        prompt_display_buffer = ">> Speed set to " + command_line + "ms. Enter command: ";
                    } else {
                        std::lock_guard<std::mutex> lock(prompt_mutex);
                        prompt_display_buffer = ">> Invalid speed. Enter a positive number: ";
                        continue; // Stay in WAITING_SPEED state
                    }
                } catch (const std::exception&) {
                    std::lock_guard<std::mutex> lock(prompt_mutex);
                    prompt_display_buffer = ">> Invalid speed. Enter a positive number: ";
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
                    command_cv.notify_all();
                }
                else {
                    std::lock_guard<std::mutex> lock(prompt_mutex);
                    prompt_display_buffer = ">> Unknown command '" + command_line + "'. Type 'help' for available commands: ";
                }
            }
        }
    }

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