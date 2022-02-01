# Star-Adventurer-Dec-Ditherer

Automatic dithering of the declination axis of a Star Adventurer using shutter release signal.

The device listens to the shutter release signal going to a camera and randomly dithers the DEC bracket between exposures. This has so far been tested working with PHD2 guiding/dithering in RA with this device dithering in DEC immediately after an exposure finishes.

*ST-4 option was planned, but I did not fully implement it. The device is capable of detecting DEC+ and DEC- pulses and should be able to use this as a trigger to dither the motor.

![Assembled PCB with 3d Printed Case](https://github.com/jconenna/Star-Adventurer-Dec-Ditherer/blob/main/images/sa_dd.jpg?raw=true)

## Description
