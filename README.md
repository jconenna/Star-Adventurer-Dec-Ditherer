# Star-Adventurer-Dec-Ditherer

Automatic dithering of the declination axis of a Star Adventurer using shutter release signal.

The device listens to the shutter release signal going to a camera and randomly dithers the DEC bracket between exposures. This has so far been tested working with PHD2 guiding/dithering in RA with this device dithering in DEC immediately after an exposure finishes.

*ST-4 option was planned, but I did not fully implement it. The device is capable of detecting DEC+ and DEC- pulses and should be able to use this as a trigger to dither the motor.

![Assembled PCB with 3d Printed Case](https://github.com/jconenna/Star-Adventurer-Dec-Ditherer/blob/main/images/sa_dd.jpg?raw=true)

## Description

A 3d printed bracket is used to attach a 28BYJ-48 stepper motor to the dec slow motion shaft.


## Bill of Materials

PCB Components:
1 - Arduino Nano Module (Classic version, check pinout carefully for compatability)

1 - RGB Character 1602 LCD Display Module

2 - PJ-307 3.5 mm Audio Jack Connector PCB Mount Female Socket 5 Pin
1 - KY-040 360 Degree Rotary Encoder Module with Knob Cap (threaded shaft with nut)
1 - 5.5mm x 2.1mm DC 3 Pin PCB Mounting DC-005
2 - Fielect 2 Position 3Pin 1P2T Panel Mount Micro Slide Switch 6mm Handle Height SK-12F44 
1 - JST 2.54MM XH 5 Pin Right Angle Connector plug 
1 - ULN2003APG 16 pin DIP Package
1 - 7805 TO-220 Package
1 - TIP120 TO-220 Package
2 - 10 kOhm Axial Resistor
1 - 0.22uF Electrolytic Capacitor
1 - 10UF Electrolytic Capacitor
1 - 100 nF Ceramic Capacitor
2 - RJ11 Modular Jack 6P6C 6 Pins PCB Mount Telephone Connector **optional**
1 - 16 pin DIP socket **optional**

Dec Components:
1 - 28BYJ-48 5V Stepper Motor
2 - M4 screws/nuts, or equivalent to mount motor
1 - 5mm-7mm Flexible Coupler Diameter 19mm Length 25mm 

3D Printed:
1 - Dec Bracket
1 - Case Top
1 - Case Bottom

PCB
1 - Dec Ditherer PCB



... work in progress ...
