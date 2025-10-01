# CSOPESY-MCO2-Group-9

## Project: CSOPESY Marquee Console

This project is a multithreaded console-based Marquee Operator written in C++. It demonstrates the use of threads, synchronization, and console manipulation to create a scrolling marquee text effect, with a command interpreter for user interaction.

## Features
- Multithreaded design: separate threads for marquee logic, display, and keyboard input
- Console-based scrolling marquee text
- Command interpreter for user input (e.g., start/stop marquee, change speed, exit)
- Customizable marquee message and speed
- Clean thread synchronization and safe shutdown

## Installation & Build
1. Open the solution `CSOPESY-MCO2-Marquee_Console.sln` in Visual Studio (Windows)
2. Build the solution (supports Debug/Release, x86/x64)
3. The main source files are in `CSOPESY-MCO2-Group 9/CSOPESY-MCO2-Marquee_Console/`

## Usage
1. Run the compiled executable (e.g., `OS_Marquee_template_code.exe`)
2. Use the command prompt at the bottom of the console to enter commands:
	- `help` : SShow list of commands
    - `start_marquee` : Start the marquee
	- `stop_marquee` : Stop the marquee
    - `set_text` : Change the marquee message
	- `set_speed` : Set marquee speed in milliseconds
	- `exit` : Quit the program

## Example
```
>> start
>> speed 100
>> set Hello, World!
>> stop
>> exit
```

## Contributors
- Ivan Antonio T. Alvarez
- Bahir Benjamin C. Barlaan
- Joshua Benedict B. Co
- Reyvin Matthew T. Tan

## License
This project is for educational purposes (CSOPESY MCO2).
