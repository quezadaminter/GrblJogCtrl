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

# Schematic
https://github.com/quezadaminter/GrblJogCtrlSchematic  

