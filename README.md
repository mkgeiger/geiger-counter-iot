# Geiger Mueller counter for IoT

## Overview

This project describes how I built a Geiger Mueller (GM)-counter as an IoT device. I found in an old russian GM-counter (from the days after the Chernobyl disaster) the 2 builtin GM-tubes of type SBM-20u (СБМ-20). The LCD display of this GM-counter was broken and so I decided to build a state of the art GM-counter which is integrated into my home-network. The features for my needs are:
* a portable device for measuring radioactive (sample) materials.
* a piezo buzzer shall indicated a gamma or beta radiation hit. It shall be possible to witch this off for long term measurements.
* also a stationary device for doing long term measurements with for at least 2 years permanent data logging.
* configurable WIFI access parameters.
* a WEB server presenting the actual ionizing radiation dose and a graph of the last hour dose.
* download of the logged data files with a format which can easily imported into LibreOffice Calc or equivalent.
* a stabilized high voltage booster to supply the GM-tubes.
* low power consumption and posibility to power the device over a mobile phone with an USB cable.

## Hardware

### GM-Tube

As already mentioned the GM-tubes are taken from a defective GM-counter. [SBM-20](/hardware/SBM-20_STS-5.jpg) tubes very popular and cheap and can be obtained at Ebay for about 20€ per piece. This GM-tube is sensitive only for beta- and gamma-radiation. It will not detect alpha-radiation. Those tubes are much more expensive. Most GM-tubes as also the SBM-20 operate well at 400V. Some other GM-tubes operate at 500V. In that case the high voltage booster has to be redesigned. Also multiple GM-tubes can operate in parallel with only one booster (but each GM-tube with its own anode resistor of about 5.1MOhm). You will get a better median with multiple tubes. The macros for SBM20_FACTOR and NBR_GMTUBES have to be adapted accordingly.

### NodeMcu ESP8266-E12

I decided to take this microcontroller board because of its power, easy programming, WIFI and small form factor. Make sure you have an NodeMcu with 4 MByte flash, which is used for long term measurement. Following GPIOs are used:

  Signal  | ESP8266 | NodeMcu
  --------|---------|--------
  Mode    | GPIO4   | D2
  GMpulse | GPIO13  | D7
  Piezo   | GPIO15  | D8
  3.3V    | 3.3V    | 3.3V
  GND     | GND     | GND
  
This module can be obtained at Ebay for about 3€.  
  
### High voltage booster

This booster is boosting 3.3V up to 400V! The 400V are regulated and stabilized with help of the z-diodes and T2. By try and error the values of the z-diodes are determined to reach exactly the 400V. For the oscillator it is important to use the CMOS variant. T3 has to support at least 400V UCE. D1 has to be fast switching diode. C2 has to be 1kV type. The anode resistor I splitted up into 2 resistors because of the high voltage drop. Best is to use exactly the components mentioned in the schematic to build up a working high voltage booster. All necessary parts can be obtained from Reichelt.

## Software

## Photos

