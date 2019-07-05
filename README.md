# Smart Thermostat
Smart thermostat for furnace and air conditioner + alarm system.

Components:
- Arduino C++ sketch running on an ESP8266EX D1 Mini from Lolin running at 160MHz
- Raspberry + Home Assistant for Web GUI, automations and MQTT server
- Bosch BME680 environmental sensor (temp, humidity, air quality, air pressure)
- SR501 PIR sensor for motion detection
- TTP223 capacitive touch buttons
- SD1306 OLED 128x64 pixel 0.96"
- 1000uf capacitor for 5V power stabilization
- 5V 220V relè
- Google Home Mini for Voice Recognition

![alt text](https://github.com/sblantipodi/smart_thermostat/blob/master/smartostat_bb.png)
