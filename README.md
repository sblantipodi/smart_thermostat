# Smart Thermostat by Davide Perini
Smart thermostat for furnace and air conditioner management,
home alarm system, light management and ambient statistics (Temperature, Humidity, Pressure, Indoor Air Quality)

Components:
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

![CIRCUITS](https://github.com/sblantipodi/smart_thermostat/blob/master/photos/smartostat_bb.png)


Smartostat YouTube video

[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/Hdy5gpQMbEk/0.jpg)](https://www.youtube.com/watch?v=Hdy5gpQMbEk)

Smartoled desk controller YouTube video

[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/_rEGXzI-NMo/0.jpg)](https://www.youtube.com/watch?v=_rEGXzI-NMo)

![PHOTO1](https://github.com/sblantipodi/smart_thermostat/blob/master/photos/1.jpg)
![PHOTO2](https://github.com/sblantipodi/smart_thermostat/blob/master/photos/2.jpg)
![PHOTO3](https://github.com/sblantipodi/smart_thermostat/blob/master/photos/3.jpg)
![PHOTO4](https://github.com/sblantipodi/smart_thermostat/blob/master/photos/4.jpg)

BOSCH Sensortec BME680 with permanent Jumper for I2C 0x76 channel

![PHOTO5](https://github.com/sblantipodi/smart_thermostat/blob/master/photos/5.jpg)

Home Assistant Mobile Client Screenshots

![PHOTO5](https://github.com/sblantipodi/smart_thermostat/blob/master/photos/ha_smartostat_screenshot.jpg)
![PHOTO6](https://github.com/sblantipodi/smart_thermostat/blob/master/photos/ha_smartostat_screenshot_2.jpg)
