# Smart Thermostat 

Smart thermostat for furnace and air conditioner management,
home alarm system, light management and ambient statistics (Temperature, Humidity, Pressure, Indoor Air Quality)  
_Written for Arduino IDE and PlatformIO._

[![GitHub version](https://img.shields.io/github/v/release/sblantipodi/smart_thermostat.svg)](https://img.shields.io/github/v/release/sblantipodi/smart_thermostat.svg)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Maintenance](https://img.shields.io/badge/Maintained%3F-yes-green.svg)](https://GitHub.com/sblantipodi/smart_thermostat/graphs/commit-activity)
[![DPsoftware](https://img.shields.io/static/v1?label=DP&message=Software&color=orange)](https://www.dpsoftware.org)

If you like **Smart Thermostat**, give it a star, or fork it and contribute!

[![GitHub stars](https://img.shields.io/github/stars/sblantipodi/smart_thermostat.svg?style=social&label=Star)](https://github.com/sblantipodi/smart_thermostat/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/sblantipodi/smart_thermostat.svg?style=social&label=Fork)](https://github.com/sblantipodi/smart_thermostat/network)

Project is bootstrapped with [Arduino Bootstrapper](https://github.com/sblantipodi/arduino_bootstrapper)

## STL Files
[Smartostat/Smartoled STL files](https://github.com/sblantipodi/smart_thermostat/tree/master/data/stl_files)

## Credits
- Davide Perini

## Components:
- Arduino C++ sketch running on an ESP8266EX D1 Mini from Lolin running @ 160MHz
- Raspberry + Home Assistant for Web GUI, automations and MQTT server
- Bosch BME680 environmental sensor (temp, humidity, air quality, air pressure)
- SR501 PIR sensor for motion detection
- TTP223 capacitive touch buttons
- SD1306 OLED 128x64 pixel 0.96"
- 1000uf capacitor for 5V power stabilization
- 5V 220V relè used to turn ON/OFF furnance
- IR emitter/receiver to manage Air Conditioner (you can use a simple IR LED, with no transistor/capacitor IR range is reduced)
- Google Home Mini for Voice Recognition

## Schematic
![CIRCUITS](https://github.com/sblantipodi/smart_thermostat/blob/master/data/img/fritzing_hardware_project.png)

## Smartostat YouTube video (Click to watch it on YouTube)
[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/Hdy5gpQMbEk/0.jpg)](https://www.youtube.com/watch?v=Hdy5gpQMbEk)

## Smartoled desk controller YouTube video (Click to watch it on YouTube)
[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/_rEGXzI-NMo/0.jpg)](https://www.youtube.com/watch?v=_rEGXzI-NMo)

![PHOTO1](https://github.com/sblantipodi/smart_thermostat/blob/master/data/img/1.jpg)
![PHOTO2](https://github.com/sblantipodi/smart_thermostat/blob/master/data/img/2.jpg)
![PHOTO3](https://github.com/sblantipodi/smart_thermostat/blob/master/data/img/3.jpg)
![PHOTO4](https://github.com/sblantipodi/smart_thermostat/blob/master/data/img/4.jpg)

## BOSCH Sensortec BME680 with permanent Jumper for I2C 0x76 channel
![PHOTO5](https://github.com/sblantipodi/smart_thermostat/blob/master/data/img/5.jpg)

## Home Assistant Mobile Client Screenshots
![PHOTO5](https://github.com/sblantipodi/smart_thermostat/blob/master/data/img/ha_smartostat_screenshot.jpg)
![PHOTO6](https://github.com/sblantipodi/smart_thermostat/blob/master/data/img/ha_smartostat_screenshot_2.jpg)

## Home Assistant Desktop Client
![PHOTOt](https://github.com/sblantipodi/smart_thermostat/blob/master/data/img/smartostat_dashboard.jpg)

## License
This program is licensed under MIT License

