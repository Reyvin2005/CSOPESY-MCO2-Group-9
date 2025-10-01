# CSOPESY-MCO2-Group-9

## Project: CSOPESY Marquee Console

This project is a multithreaded console-based Marquee Operator written in C++. It demonstrates the use of threads, synchronization, and console manipulation to create a scrolling marquee text effect, with a command interpreter for user interaction.

## Features
- Multithreaded design: separate threads for marquee logic animation, display, keyboard input, and console resize monitoring
- Console-based scrolling marquee text with colorized UI
- Command interpreter for user input (help, start_marquee, stop_marquee, set_text, set_speed, exit)
- Customizable marquee message and speed
- Clean thread synchronization and safe shutdown
- Responsive UI that adapts to console resizing

## Installation & Build
1. Open the solution `CSOPESY-MCO2-Marquee_Console.sln` in Visual Studio 2022 (Windows)
2. Build the solution (supports Debug/Release, x86/x64)
3. The main source files are in `CSOPESY-MCO2-Group 9/CSOPESY-MCO2-Marquee_Console/`

## Usage
1. Run the compiled executable (e.g., `Group 9_OS_Marquee_Console.exe`)
2. Use the command prompt at the bottom of the console to enter commands:
	- `help` : SShow list of commands
    - `start_marquee` : Start the marquee animation
	- `stop_marquee` : Stop the marquee animation
    - `set_text` : Change the marquee message (prompts for input)
	- `set_speed` : Set marquee speed in milliseconds (prompts for input)
	- `exit` : Quit the program

## Example
```
>> set_text
	(after pressing enter,
	 type your desired message) Hello, World!
>> set_speed
	(after pressing enter,
	 type your desired speed) 100
>> start_marquee
>> stop_marquee
>> exit
```

## Contributors
- Ivan Antonio T. Alvarez
- Bahir Benjamin C. Barlaan
- Joshua Benedict B. Co
- Reyvin Matthew T. Tan

## License
This project is for educational purposes (CSOPESY MCO2).
