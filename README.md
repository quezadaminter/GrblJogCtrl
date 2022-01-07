# GrblJogCtrl
Grbl hand-held controller firmware.  

Works as a DRO when used in MITM (man in the middle) mode. That is, when gcode is being sent from another system, typically a computer running a gcode sender program.  
Also works on its own, sending commands to GRBL from the buttons and the encoder, allowing the user to drive the machine states themselves.  
Beyond that, it can read gcode from files placed on a SD card. The controller can then stream the code to GRBL, avoiding the need for a separate sender.

![Screenshot](1.gif)

Demo videos:  
https://www.youtube.com/watch?v=UvRLiaFA8Ps  
https://www.youtube.com/watch?v=HVsqZHhviag  
https://www.youtube.com/watch?v=aRfksGpZmzs 

# Parts Used
1- Teensy 4.1 Development Board.  
1- ILI9488 LCD https://www.buydisplay.com/serial-spi-3-5-inch-tft-lcd-module-in-320x480-optl-touchscreen-ili9488  
1- 50 pin LCD breakout adapter board. Link in the schematic.  
1- 12 Button key pad, I made my own but there are many options out there.  
1- Shift register IC to read keypad.  
2- 12 position rotary selector switches (12PST).  
1- Female USB 2.0 type A connector.  
1- 20 pulse rotary encoder with momentary switch. Feel free to substitute for one with a higher resolution.  

# Schematic
https://github.com/quezadaminter/GrblJogCtrlSchematic  

# Building the source
You will need to install the Arduino IDE and follow the instructions from PJRC to install the Teensyduino add-on. The instructions can be found here https://www.pjrc.com/teensy/teensyduino.html.

I use and set up the project with VSCode using the VisualTeensy utility (https://github.com/luni64/VisualTeensy).

If you want/need to mess with the source code I recommend doing it from VSCode. This is a richer IDE that makes it easier (at least for me) to manage the project. It does come with a bit of work setting it up as outlined below. So for minor work or changes in the code it may be worth dealing with the Arduino IDE.

## Library Dependencies
Some libraries are needed to build and link the program. Here is the list with the path ON MY SYSTEM to them. If you install on different paths make sure you check and update the .vscode/c_cpp_properties.json file

-**Teensy4** core: Arduino/hardware/teensy/avr/cores/teensy4  
-**ILI9488_t3** For the display: Arduino/libraries/ILI9488_t3 (Download from https://github.com/mjs513/ILI9488_t3)  
-**SPI** If using the SPI display: C:/Program Files (x86)/Arduino/hardware/teensy/avr/libraries/SPI  
-**USBHost** To enable the board to serve as a USB Host: C:/Program Files (x86)/Arduino/hardware/teensy/avr/libraries/USBHost_t36  
-**SD** To manage the SD Card: C:/Program Files (x86)/Arduino/hardware/teensy/avr/libraries/SD  

## VSCode
If you are using VSCode, follow make sure you installed the C++ extension. You can open this project from VSCode using the XC-Pendant-workspace.code-workspace file in the src/ directory. You will also need to open the .vscode/c_cpp_properties.json, .vscode/launch.json and .vscode/tasks.json files to modify the paths to the relevant tools and libraries.

## Arduino (easier)
Once you have your Arduino IDE set up and working with Teensyduino you can load the sketch. To do this, open the src/src.ino file. Unfortunately the Arduino IDE requires that the name of the ino file match the name of the directory it lives in, hence it being called src.ino. It should load into the IDE and you should be able to build. Make sure you selected the correct board type and configuration: 

-Board: Teensy 4.1  
-USB Type: Serial  
-CPU Speed: 600 MHz (This is how I use and test it)  
-Optimize: Faster  
-GDB: off (unless you know what you are doing)  
