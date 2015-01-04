---
title: TeleBall - Electronics
layout: default
imgfolder: eagle
pcb_image:
  - name: pcb_large.jpg
    thumb: pcb_small.jpg
    text: TeleBall PCB Top (left) & Bottom (right)
---

{% include gallery_init.html %}

TeleBall Electronics
====================

Schematics
----------

![Schematics Overview](eagle/circuit.png)
Abbreviations:

* BAT+ / BAT-: Battery+ and Battery- aka GND, directly wired via battery clips
* +UB: 6V battery power *after* the on/off switch
* ARDUINO: Microcontroller - see details below
* SWITCH: TeleBall power switch / on/off switch
* R5, R6, R7: 8.2kΩ series resistor for LEDs

Download [Detailed Schematics PDF](eagle/BreakOut-8x8-rev2-circuit.pdf).

### Main parts

* Microcontroller: 5V version of Arduino Nano 3.0 or compatible device ([Data Sheet](http://arduino.cc/de/Main/ArduinoBoardNano))
* 8x8 LED Display: KWM-R30881CBB aka AdaFruit 1817 ([Data Sheet](http://www.adafruit.com/datasheets/1047datasheet.pdf))
* LED Display Driver: MAX7221    ([Data Sheet](http://datasheets.maximintegrated.com/en/ds/MAX7219-MAX7221.pdf))
* Radio: nRF24L01+ on Arduino friendly breakout board ([Data Sheet](http://www.nordicsemi.com/eng/content/download/2726/34069/file/nRF24L01P_Product_Specification_1_0.pdf))




Custom Printed Circuit Board (PCB)
----------------------------------

{% include gallery_show.html images=page.pcb_image group=2 %}