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

As already mentioned the GM-tubes are taken from a defective GM-counter. SBM-20 tubes very popular and cheap and can be obtained at Ebay for about 20€ per piece. This GM-tube is sensitive only for beta- and gamma-radiation. It will not detect alpha-radiation. Those tubes are much more expensive. Most GM-tubes as also the SBM-20 operate well at 400V. Some other GM-tubes operate at 500V. In that case the high voltage booster has to be redesigned. Also multiple GM-tubes can operate in parallel with only one booster (but each GM-tube with its own anode resistor of about 5.1MOhm). You will get a better median with multiple tubes. The macros for SBM20_FACTOR and NBR_GMTUBES have to be adapted accordingly. In the case I preferred to leave a window open to bring radioactive samples very close to the GM-tubes.

* [SBM-20 english datasheet](/datasheets/SBM-20_ENG.pdf)
* [SBM-20 german datasheet](/datasheets/SBM-20_GER.pdf)

![SBM-20](/hardware/SBM-20_STS-5.jpg) 

### NodeMcu ESP8266-12E

I decided to take this microcontroller board because of its power, easy programming, WIFI and small form factor. Make sure you have an NodeMcu with 4 MByte flash, which is used for long term measurement. Following GPIOs are used:

  Signal  | ESP8266 | NodeMcu
  --------|---------|--------
  Mode    | GPIO4   | D2
  GMpulse | GPIO13  | D7
  Piezo   | GPIO15  | D8
  3.3V    | 3.3V    | 3.3V
  GND     | GND     | GND
  
This module can be obtained at Ebay for about 3€.  
  
![NodeMcu](/hardware/NodeMcu_ESP8266_12E.png)  
  
### High voltage booster

This power supply is boosting 3.3V up to 400V! The 400V output is regulated and stabilized with help of the z-diodes and transistor T2. By trial and error the values of the z-diodes were determined to reach exactly the 400V. For the oscillator it is important to use the CMOS variant. Transistor T3 has to support at least 400V UCE. Diode D1 has to be a fast switching diode. Capacitor C2 has to be a 1kV type. The anode resistor I splitted up into 2 resistors because of the high voltage drop. I would suggest to use exactly the components mentioned in the schematic to build up a working high voltage booster. All necessary parts can be obtained from [Reichelt](https://www.reichelt.de/).

![Schematic](/hardware/400V_Booster.png)

### Mode Switch

The mode switch is used to switch between long term measurement mode (with piezo buzzer switched off) and portable mode (with piezo buzzer switched on).

  Mode switch    | Piezo buzzer
  ---------------|-------------
  long term mode | off
  portable mode  | on

## Software

### Preparations

First install the [Arduino IDE](https://www.arduino.cc/en/main/software). Under board administration install support for "NodeMCU 1.0 (ESP-12E Module)". The flash size has to be configured for "4M (3M SPIFFS)". The CPU frequency has to be configured for "160 MHz". Following libraries have to be installed:

* Time
* WiFi
* WiFiManager

### GM-tube parameterization

The GM-tube conversion factor can be adapted in the source code when other GM-tubes are used. In the datasheet of the SBM-20 tube we find following: Gamma Sensivity Ra226 = 29 cps/mR/h. In other words: 29 cps are equivalent to 1 mR/h. Or 1740 cpm are equivalent to 10μS/h. Or 1 cpm is equivalent to 0.0057 μS/h. This conversion factor has to be divided by the number of used GM-tubes.

### NTP client

The NTP client adjust the internal clock every 10 minutes. The timestamp (UTC Unix time) is used for the filenames containing the logging data and also for each logging event.

### HTTP server

The HTTP server shows a webpage, which is refreshing its contents every minute or on request by the "Refresh" button, with following contents:

* the session details (total hit count, total minutes, average CPM and dose) after the last power reset or pressing the "Reset Session" button.
* a link to the download page.
* a graphical gauge to display the current dose of the last minute. The gauge is adjusting its range for higher doses.
* a diagram to show the CPM and dose values of the last hour after reset.
* the free space in the filesystem
* a "Reset Wifi" button to enter the access point mode to be able to modify the Wifi parameters.

![Screenshot](/photos/screenshot.png)

### Piezo buzzer

The piezo buzzer is switched on only in portable mode. Best sound results for the ticks appeared when the buzzer is switched on for only 1 ms.

### Data logging

For data logging the SPIFFS formated file system is used. When the "Clear"-Button in the download page is pressed all data logging files are deleted except the current one. The median value of the last 10 min (10 measurements @ 1 measurement/min) is logged together with its Unix timstamp. With this logging rate it is possible to log permanently for more than 2 years. The user has to download and delete older logfiles from time to time. One logfile contains the data of 4 weeks.

## Photos

Opened case:
![Opened](/photos/opened.jpg)

NodeMcu ESP8266-12E:
![Opened](/photos/esp8266.jpg)

High voltage booster:
![Opened](/photos/highvoltage.jpg)

Finished device:
![Opened](/photos/finished.jpg)
