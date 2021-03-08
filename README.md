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

