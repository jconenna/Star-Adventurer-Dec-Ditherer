# Star-Adventurer-Dec-Ditherer

Automatic dithering of the declination axis of a Star Adventurer using shutter release signal.

The device listens to the shutter release signal going to a camera and randomly dithers the DEC bracket between exposures. This has so far been tested working with PHD2 guiding/dithering in RA with this device dithering in DEC immediately after an exposure finishes.

*ST-4 option was planned, but I did not fully implement it. The device is capable of detecting DEC+ and DEC- pulses and should be able to use this as a trigger to dither the motor.

![Assembled PCB with 3d Printed Case](https://github.com/jconenna/Star-Adventurer-Dec-Ditherer/blob/main/images/sa_dd.jpg?raw=true)

## Description

A 3d printed bracket is used to attach a 28BYJ-48 stepper motor to the Dec slow motion shaft using M4 screws/nuts.</br>
A flexible coupler is used to connect the motor shaft to the slow motion shaft.</br>

![Dec Assembly 1](https://github.com/jconenna/Star-Adventurer-Dec-Ditherer/blob/main/images/dec1.jpg?raw=true)

The motor bracket attaches to the bottom of the Dec block using existing screws, with some of the other existing screws removed.

![Dec Assembly 2](https://github.com/jconenna/Star-Adventurer-Dec-Ditherer/blob/main/images/dec2.jpg?raw=true)

The controlling device has a red backlit LCD screen and move/select knob for the user interface.

![Dec Assembly 2](https://github.com/jconenna/Star-Adventurer-Dec-Ditherer/blob/main/images/unit1.jpg?raw=true)

## Bill of Materials

PCB Components:<br/>
1 - Arduino Nano Module (Classic version, check pinout carefully for compatability)<br/>
1 - RGB Character 1602 LCD Display Module<br/>
2 - PJ-307 3.5 mm Audio Jack Connector PCB Mount Female Socket 5 Pin<br/>
1 - KY-040 360 Degree Rotary Encoder Module with Knob Cap (threaded shaft with nut)<br/>
1 - 5.5mm x 2.1mm DC 3 Pin PCB Mounting DC-005<br/>
2 - Fielect 2 Position 3Pin 1P2T Panel Mount Micro Slide Switch 6mm Handle Height SK-12F44 <br/>
1 - JST 2.54MM XH 5 Pin Right Angle Connector plug <br/>
1 - ULN2003APG 16 pin DIP Package<br/>
1 - 7805 TO-220 Package<br/>
1 - TIP120 TO-220 Package<br/>
2 - 10 kOhm Axial Resistor<br/>
1 - 0.22uF Electrolytic Capacitor<br/>
1 - 10UF Electrolytic Capacitor<br/>
1 - 100 nF Ceramic Capacitor<br/>
2 - RJ11 Modular Jack 6P6C 6 Pins PCB Mount Telephone Connector **optional**<br/>
1 - 16 pin DIP socket **optional**<br/>
<br/>
Dec Components:<br/>
1 - 28BYJ-48 5V Stepper Motor<br/>
2 - M4 screws/nuts, or equivalent to mount motor<br/>
1 - 5mm-7mm Flexible Coupler Diameter 19mm Length 25mm <br/>
<br/>
3D Printed:<br/>
1 - Dec Bracket<br/>
1 - Case Top<br/>
1 - Case Bottom<br/>
<br/>
PCB<br/>
1 - Dec Ditherer PCB<br/>

<br/>

... work in progress ...
