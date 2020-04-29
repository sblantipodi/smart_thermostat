/*
  Smartostat.h - Smart Thermostat based on Arduino SDK
  
  Copyright (C) 2020  Davide Perini
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of 
  this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
  copies of the Software, and to permit persons to whom the Software is 
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in 
  all copies or substantial portions of the Software.
  
  You should have received a copy of the MIT License along with this program.  
  If not, see <https://opensource.org/licenses/MIT/>.

  * Components:
  - Arduino C++ sketch running on an ESP8266EX D1 Mini from Lolin running at 160MHz
  - Raspberry + Home Assistant for Web GUI, automations and MQTT server
  - Bosch BME680 environmental sensor (temp, humidity, air quality, air pressure)
  - SR501 PIR sensor for motion detection
  - TTP223 capacitive touch buttons
  - SD1306 OLED 128x64 pixel 0.96"
  - 1000uf capacitor for 5V power stabilization
  - 5V 220V relè
  - Google Home Mini for Voice Recognition
*/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#ifdef TARGET_SMARTOSTAT
  #include <Adafruit_Sensor.h>
  #include <Adafruit_BME680.h>
  #include <IRremoteESP8266.h>
  #include <IRsend.h>
  #include <ir_Samsung.h>
#endif
#include "Version.h"
#include "Configuration.h"
#include "BootstrapManager.h"


/****************** BOOTSTRAP and WIFI MANAGER ******************/
BootstrapManager bootstrapManager;
Helpers helper;

/**************************** PIN DEFINITIONS **************************************************/
// #define OLED_RESET LED_BUILTIN // Pin used for integrated D1 Mini blue LED
#ifdef TARGET_SMARTOSTAT
  #define OLED_BUTTON_PIN 12 // D6 Pin, 5V power, for capactitive touch sensor. When Sig Output is high, touch sensor is being 
#else
  #define OLED_BUTTON_PIN 14 // D5 Pin, 5V power, for capactitive touch sensor. When Sig Output is high, touch sensor is being 
#endif
#define SMARTOSTAT_BUTTON_PIN 15 // D8 pin, 5V power, for touch button used to start stop the furnance 
#ifdef TARGET_SMARTOSTAT
  #define SR501_PIR_PIN 16 // D0 Pin, 5V power, for SR501 PIR sensor
  #define RELE_PIN 14 // D5 Pin, 5V power, for relè //TODO configurare releè pin
  Adafruit_BME680 boschBME680; // D2 pin SDA, D1 pin SCL, 3.3V power for BME680 sensor, sensor address I2C 0x76
  const uint16_t kIrLed = D3; // GPIOX for IRsender
  IRSamsungAc acir(kIrLed);  
#endif

// #define SCREEN_WIDTH 128 // OLED display width, in pixels
// #define SCREEN_HEIGHT 64 // OLED display height, in pixels
// // Declaration for an SSD1306 display connected to I2C (SDA, SCL pins) // Address 0x3C for 128x64pixel
// // D2 pin SDA, D1 pin SCL, 5V power 
// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); 

/************* MQTT TOPICS **************************/
const char* SMARTOSTAT_SENSOR_STATE_TOPIC = "tele/smartostat/SENSOR";
const char* SMARTOSTAT_STATE_TOPIC = "tele/smartostat/STATE";
const char* SMARTOSTAT_CLIMATE_STATE_TOPIC = "stat/smartostat/CLIMATE";
const char* SMARTOSTATAC_CMD_TOPIC = "cmnd/smartostatac/CLIMATE";
const char* SMARTOSTAT_FURNANCE_STATE_TOPIC = "stat/smartostat/POWER1";
const char* SMARTOSTAT_PIR_STATE_TOPIC = "stat/smartostat/POWER2";
const char* SPOTIFY_STATE_TOPIC = "stat/spotify/info";
const char* SMARTOSTAT_FURNANCE_CMND_TOPIC = "cmnd/smartostat/POWER1";
const char* SMARTOSTATAC_STAT_IRSEND = "stat/smartostatac/IRsend";
const char* SMARTOSTATAC_CMND_IRSEND = "cmnd/smartostatac/IRsendCmnd";
const char* SMARTOSTAT_CMND_CLIMATE_HEAT_STATE = "cmnd/smartostat/climateHeatState";
const char* SMARTOSTAT_CMND_CLIMATE_COOL_STATE = "cmnd/smartostat/climateCoolState";
const char* UPS_STATE = "stat/ups/INFO";
const char* SOLAR_STATION_POWER_STATE = "stat/solarstation/POWER";
const char* SOLAR_STATION_PUMP_POWER = "stat/water_pump/POWER";
const char* SOLAR_STATION_STATE = "tele/solarstation/STATE";

#ifdef TARGET_SMARTOSTAT
  const char* SMARTOLED_CMND_TOPIC = "cmnd/smartostat_oled/POWER3";
  const char* SMARTOLED_STATE_TOPIC = "stat/smartostat_oled/POWER3";
  const char* SMARTOLED_INFO_TOPIC = "stat/smartostat_oled/INFO";
  const char* SMARTOSTATAC_CMND_IRSENDSTATE = "cmnd/smartostatac/IRsend";
  const char* SMARTOSTAT_STAT_REBOOT = "stat/smartostat/reboot";
  const char* SMARTOSTAT_CMND_REBOOT = "cmnd/smartostat/reboot";
  const char* SPIFFS_STATE = "stat/smartostat/SPIFFS";  
#endif
#ifdef TARGET_SMARTOLED
  const char* SMARTOLED_CMND_TOPIC = "cmnd/smartoled/POWER3";
  const char* SMARTOLED_STATE_TOPIC = "stat/smartoled/POWER3";
  const char* SMARTOLED_INFO_TOPIC = "stat/smartoled/INFO";
  const char* SMARTOLED_STAT_REBOOT = "stat/smartoled/reboot";
  const char* SMARTOLED_CMND_REBOOT = "cmnd/smartoled/reboot";
  const char* SPIFFS_STATE = "stat/smartoled/SPIFFS";  
#endif 

// HEAT COOL THRESHOLD, USED to MANAGE SITUATIONS WHEN THERE IS NO INFO FROM THE MQTT SERVER (used by smartoled for capacitive button too)
const int HEAT_COOL_THRESHOLD = 20;

// Display state
bool stateOn = true;

// Button state variables
byte buttonState = 0;
byte  lastReading = 0;
unsigned long onTime = 0;
bool longPress = false;
bool veryLongPress = false;

// Humidity Threshold
float humidityThreshold = 75;
float tempSensorOffset = 0;

// Total Number of pages
const int numPages = 9;
const float LOW_WATT = 250;
const float HIGH_WATT = 350;
String loadwatt = OFF_CMD;
float loadwattMax = 0;
float loadFloatPrevious = 0;
String runtime = OFF_CMD;
String inputVoltage = OFF_CMD;
String outputVoltage = OFF_CMD;
String temperature = OFF_CMD;
float minTemperature = 99;
float maxTemperature = 0.0;
String humidity = OFF_CMD;
float minHumidity = 99;
float maxHumidity = 0.0;
String pressure = OFF_CMD;
float minPressure = 2000;
float maxPressure = 0.0;
String gasResistance = OFF_CMD;
float minGasResistance = 2000;
float maxGasResistance = 0.0;
String IAQ = OFF_CMD; // indoor air quality
float minIAQ = 2000;
float maxIAQ = 0.0;
String furnance = OFF_CMD;
String ac = OFF_CMD;
String pir = OFF_CMD;
String target_temperature = OFF_CMD;
String hvac_action = OFF_CMD;
String COOL = "cooling";
String HEAT = "heating";
String IDLE = "idle";
String fan = OFF_CMD;
String FAN_QUIET = "Quiet";
String FAN_LOW = "Low";
String FAN_HIGH = "High";
String FAN_AUTO = "Auto";
String FAN_POWER = "Power";

String SPOTIFY_PLAYING = "playing";
String SPOTIFY_IDLE = "idle";
String SPOTIFY_PAUSED = "paused";

String away_mode = OFF_CMD;
String rebootState = OFF_CMD;
String alarm = OFF_CMD;
String ALARM_ARMED_AWAY = "armed_away";
String ALARM_PENDING = "pending";
String ALARM_TRIGGERED = "triggered";
bool furnanceTriggered = false;
bool acTriggered = false;
bool ssTriggered = false;
bool wpTriggered = false;
bool ssUploadMode = false;
int ssTriggerCycle = 0;
int solarStationBattery = 0;
String solarStationBatteryVoltage = OFF_CMD;
String solarStationRemainingSeconds = OFF_CMD;
String solarStationWifi = OFF_CMD;
int currentPage = 0;
bool showHaSplashScreen = true;
String hours = EMPTY_STR;
String minutes = EMPTY_STR;
bool pressed = false;
String spotifySource = EMPTY_STR;
String volumeLevel = EMPTY_STR;
String mediaArtist = EMPTY_STR;
String mediaTitle = OFF_CMD;
String mediaTitlePrevious = OFF_CMD;
String spotifyPosition = OFF_CMD;
String spotifyPositionPrevious = OFF_CMD;
String appName = EMPTY_STR;
String spotifyActivity = EMPTY_STR;
String BT_AUDIO = "Bluetooth Audio";
int offset = 160;
int offsetAuthor = 130;
// variable used for faster delay instead of arduino delay(), this custom delay prevent a lot of problem and memory leak
const int tenSecondsPeriod = 10000;
unsigned long timeNowStatus = 0;
// five minutes
const int fiveMinutesPeriod = 300000;
unsigned long timeNowGoHomeAfterFiveMinutes = 0;
unsigned int lastButtonPressed = 0;
unsigned int delayTime = 20;

#ifdef TARGET_SMARTOSTAT
  // PIR variables
  long unsigned int highIn;
 
  unsigned int readOnceEveryNTimess = 0;
  String lastPirState = OFF_CMD;
  float hum_weighting = 0.25; // so hum effect is 25% of the total air quality score
  float gas_weighting = 0.75; // so gas effect is 75% of the total air quality score
  int   humidity_score, gas_score;
  float gas_reference = 2500;
  float hum_reference = 40;
  int   getgasreference_count = 0;
  int   gas_lower_limit = 10000;  // Bad air quality limit
  int   gas_upper_limit = 300000; // Good air quality limit
#endif
// only button can force furnance state to ON even when wifi/mqtt is disconnected, the force state is resetted to OFF even by MQTT topic
boolean forceFurnanceOn = false;
boolean forceACOn = false;

int SWITCHFOOTEREVERYNDRAW = 200;
int switchFooter = 0;

// 'heat', 33x29px
static const unsigned char tempLogo [] PROGMEM = {
  0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x0c, 0x01, 0xfc, 0x00, 0x00, 0x1c, 0x01, 0x86, 0x00, 0x00,
  0x1e, 0x03, 0x06, 0x00, 0x08, 0x33, 0x03, 0x26, 0x00, 0x0f, 0xe3, 0xf3, 0x26, 0x00, 0x0f, 0xe1,
  0xe3, 0x26, 0x00, 0x0c, 0x00, 0x03, 0x26, 0x00, 0x0c, 0x1c, 0x03, 0x26, 0x00, 0x0c, 0x3f, 0x03,
  0x26, 0x00, 0x0c, 0x63, 0x83, 0x26, 0x00, 0x3c, 0xc0, 0xc3, 0x26, 0x00, 0x70, 0x80, 0xc3, 0x26,
  0x00, 0xe1, 0x80, 0x43, 0x26, 0x00, 0x70, 0x80, 0xc3, 0x26, 0x00, 0x38, 0xc0, 0xc3, 0x26, 0x00,
  0x0c, 0xe1, 0x83, 0x26, 0x00, 0x0c, 0x7f, 0x03, 0x26, 0x00, 0x0c, 0x1e, 0x03, 0x26, 0x00, 0x0c,
  0x00, 0x06, 0x23, 0x00, 0x0d, 0xe1, 0xc6, 0x71, 0x80, 0x0f, 0xe3, 0xcc, 0xf9, 0x80, 0x0c, 0x33,
  0x0c, 0xf9, 0x80, 0x00, 0x3e, 0x0c, 0xf9, 0x80, 0x00, 0x1e, 0x0c, 0x79, 0x80, 0x00, 0x0c, 0x06,
  0x03, 0x00, 0x00, 0x00, 0x07, 0x07, 0x00, 0x00, 0x00, 0x03, 0xfe, 0x00, 0x00, 0x00, 0x00, 0xf8,
  0x00
};
#define tempLogoW  33
#define tempLogoH  29

// 'power-socket', 33x33px
static const unsigned char upsLogo [] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x3f, 0xfe, 0x00, 0x00, 0x00, 0x7f, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 
  0x80, 0x00, 0x01, 0xe0, 0x03, 0xc0, 0x00, 0x03, 0xc0, 0x01, 0xe0, 0x00, 0x07, 0x81, 0xc0, 0xf0, 
  0x00, 0x0f, 0x01, 0xc0, 0x78, 0x00, 0x0e, 0x01, 0xc0, 0x38, 0x00, 0x0e, 0x31, 0xc6, 0x38, 0x00, 
  0x0e, 0x31, 0xc6, 0x38, 0x00, 0x0e, 0x31, 0xc6, 0x38, 0x00, 0x0e, 0x30, 0x06, 0x38, 0x00, 0x0e, 
  0x30, 0x06, 0x38, 0x00, 0x0e, 0x30, 0x06, 0x38, 0x00, 0x0e, 0x00, 0x00, 0x38, 0x00, 0x0e, 0x00, 
  0x00, 0x38, 0x00, 0x0f, 0xff, 0xff, 0xf8, 0x00, 0x0f, 0xff, 0xff, 0xf8, 0x00, 0x0f, 0xff, 0xff, 
  0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00
};
#define upsLogoW  33
#define upsLogoH  33

// 'fire', 23x28px
static const unsigned char fireLogo [] PROGMEM = {
  0xff, 0xff, 0xfe, 0xff, 0xfb, 0xfe, 0xff, 0xe3, 0xfe, 0xff, 0xc3, 0xfe, 0xff, 0x83, 0xfe, 0xff,
  0x83, 0xfe, 0xff, 0x01, 0xfe, 0xff, 0x01, 0xfe, 0xfe, 0x00, 0xfe, 0xfe, 0x00, 0x7e, 0xf6, 0x00,
  0x3e, 0xe6, 0x00, 0x1e, 0xe6, 0x00, 0x0e, 0xc6, 0x00, 0x06, 0xc6, 0x0c, 0x06, 0xc7, 0x0e, 0x02,
  0xc3, 0x07, 0x02, 0xc0, 0x07, 0x82, 0xc0, 0x07, 0x82, 0xc0, 0x07, 0x82, 0xc0, 0x0f, 0x82, 0xe0,
  0x1f, 0x02, 0xe0, 0x3e, 0x06, 0xf0, 0x00, 0x06, 0xf8, 0x00, 0x0e, 0xfc, 0x00, 0x1e, 0xff, 0x00,
  0x7e, 0xff, 0xef, 0xfe
};
const int fireLogoW = 23;
const int fireLogoH = 28;

// 'snow', 25x29px
static const unsigned char snowLogo [] PROGMEM = {
  0xff, 0xe3, 0xff, 0x80, 0xff, 0xe3, 0xff, 0x80, 0xff, 0x22, 0x7f, 0x80, 0xff, 0x00, 0x7f, 0x80,
  0xff, 0x00, 0x7f, 0x80, 0xf3, 0x80, 0xe7, 0x80, 0xb3, 0xc1, 0xe6, 0x80, 0x81, 0xe3, 0xc0, 0x80,
  0x01, 0xe3, 0xc0, 0x00, 0x01, 0xe3, 0xc0, 0x00, 0xc0, 0x63, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x3f, 0x80, 0xff, 0x00, 0x7f, 0x80, 0xfe, 0x00, 0x3f, 0x80,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x63, 0x01, 0x80, 0x01, 0xe3, 0xc0, 0x00,
  0x01, 0xe3, 0xc0, 0x00, 0x81, 0xe3, 0xc0, 0x80, 0xb3, 0xc1, 0xe6, 0x80, 0xf3, 0x80, 0xe7, 0x80,
  0xff, 0x00, 0x7f, 0x80, 0xff, 0x00, 0x7f, 0x80, 0xff, 0x22, 0x7f, 0x80, 0xff, 0xe3, 0xff, 0x80,
  0xff, 0xe3, 0xff, 0x80
};
#define snowLogoW  25
#define snowLogoH  28

// 'cooll', 33x29px
static const unsigned char coolLogo [] PROGMEM = {
  0x00, 0x00, 0x03, 0xe0, 0x00, 0x00, 0x00, 0x07, 0xf8, 0x00, 0x00, 0x60, 0x0f, 0xfc, 0x00, 0x01,
  0x78, 0x1c, 0x1c, 0x00, 0x03, 0xfc, 0x1c, 0x0e, 0x00, 0x19, 0xf8, 0x38, 0x0e, 0x00, 0x58, 0xf0,
  0x38, 0x0e, 0x00, 0xf8, 0x60, 0x38, 0x0e, 0x00, 0xfc, 0x61, 0x38, 0x0e, 0x00, 0x7e, 0x67, 0x38,
  0x0e, 0x00, 0xff, 0xff, 0x38, 0x0e, 0x00, 0x47, 0xfe, 0x38, 0x0e, 0x00, 0x01, 0xf8, 0x38, 0x0e,
  0x00, 0x03, 0xfc, 0x38, 0x0e, 0x00, 0xff, 0xff, 0x38, 0x0e, 0x00, 0x7f, 0xff, 0x38, 0x0e, 0x00,
  0x7c, 0x63, 0x38, 0x8f, 0x00, 0xfc, 0x60, 0x70, 0xc7, 0x00, 0x78, 0x70, 0x70, 0xc3, 0x80, 0x18,
  0xf8, 0x63, 0xe3, 0x80, 0x01, 0xfc, 0x63, 0xe3, 0x80, 0x03, 0xfc, 0x63, 0xe3, 0x80, 0x00, 0x60,
  0x61, 0xe3, 0x80, 0x00, 0x00, 0x70, 0x03, 0x00, 0x00, 0x00, 0x78, 0x07, 0x00, 0x00, 0x00, 0x3c,
  0x0e, 0x00, 0x00, 0x00, 0x1f, 0xfe, 0x00, 0x00, 0x00, 0x0f, 0xf8, 0x00, 0x00, 0x00, 0x03, 0xe0,
  0x00
};
#define coolLogoW  33
#define coolLogoH  29

// 'humidi', 22x29px
static const unsigned char humidityLogo [] PROGMEM = {
  0x00, 0x30, 0x00, 0x00, 0x78, 0x00, 0x00, 0x78, 0x00, 0x00, 0xfc, 0x00, 0x00, 0xfc, 0x00, 0x01,
  0xfe, 0x00, 0x01, 0xfe, 0x00, 0x03, 0xff, 0x00, 0x07, 0xff, 0x80, 0x07, 0xff, 0x80, 0x0f, 0xff,
  0xc0, 0x1f, 0xff, 0xe0, 0x3f, 0xff, 0xf0, 0x3f, 0xff, 0xf0, 0x7f, 0xfd, 0xf8, 0x7c, 0x79, 0xf8,
  0xfc, 0x73, 0xfc, 0xfc, 0x67, 0xfc, 0xff, 0xcf, 0xfc, 0xff, 0x8f, 0xfc, 0xff, 0x98, 0xfc, 0xff,
  0x38, 0xfc, 0x7e, 0x78, 0xf8, 0x7e, 0x7d, 0xf8, 0x3f, 0xff, 0xf0, 0x1f, 0xff, 0xe0, 0x0f, 0xff,
  0xc0, 0x07, 0xff, 0x80, 0x00, 0xfc, 0x00
};
#define humidityLogoW  22
#define humidityLogoH  29

// 'off', 28x29px
static const unsigned char offLogo [] PROGMEM = {
	0x00, 0x06, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x02, 0x0f, 0x04, 0x00,
	0x07, 0x0f, 0x0e, 0x00, 0x0f, 0x0f, 0x0f, 0x00, 0x1f, 0x0f, 0x0f, 0x80, 0x3e, 0x0f, 0x07, 0xc0,
	0x3c, 0x0f, 0x03, 0xc0, 0x7c, 0x0f, 0x03, 0xe0, 0x78, 0x0f, 0x01, 0xe0, 0x78, 0x0f, 0x01, 0xe0,
	0xf0, 0x0f, 0x00, 0xf0, 0xf0, 0x0f, 0x00, 0xf0, 0xf0, 0x0f, 0x00, 0xf0, 0xf0, 0x0f, 0x00, 0xf0,
	0xf0, 0x00, 0x00, 0xf0, 0xf0, 0x00, 0x00, 0xf0, 0x78, 0x00, 0x01, 0xe0, 0x78, 0x00, 0x01, 0xe0,
	0x7c, 0x00, 0x03, 0xe0, 0x3e, 0x00, 0x07, 0xc0, 0x3f, 0x00, 0x0f, 0xc0, 0x1f, 0x80, 0x1f, 0x80,
	0x0f, 0xe0, 0xff, 0x00, 0x07, 0xff, 0xfe, 0x00, 0x01, 0xff, 0xf8, 0x00, 0x00, 0xff, 0xf0, 0x00,
	0x00, 0x1f, 0x80, 0x00
};
#define offLogoW  28
#define offLogoH   29

// 'shield', 9x10px
static const unsigned char shieldLogo [] PROGMEM = {
	0x1c, 0x00, 0x7f, 0x00, 0xf1, 0x80, 0xf0, 0x80, 0xf0, 0x80, 0xf1, 0x80, 0x71, 0x00, 0x73, 0x00,
	0x3e, 0x00, 0x1c, 0x00
};
#define shieldLogoW  9
#define shieldLogoH  10

// 'run', 8x10px
static const unsigned char runLogo [] PROGMEM = {
	0x0c, 0x0c, 0x7c, 0x7c, 0x1f, 0x3b, 0x3c, 0xec, 0x4c, 0x0c
};
#define runLogoW  8
#define runLogoH  10

// 'ha', 9x8px
static const unsigned char haSmallLogo [] PROGMEM = {
	0x08, 0x00, 0x1e, 0x00, 0x3e, 0x00, 0x77, 0x00, 0xd5, 0x80, 0x63, 0x00, 0x77, 0x00, 0x77, 0x00
};
#define haSmallLogoW  9
#define haSmallLogoH  8

// 'humid', 22x29px
static const unsigned char humidityBigLogo [] PROGMEM = {
	0x00, 0x30, 0x00, 0x00, 0x78, 0x00, 0x00, 0x78, 0x00, 0x00, 0xf8, 0x00, 0x00, 0xfc, 0x00, 0x00,
	0xfc, 0x00, 0x01, 0xfe, 0x00, 0x03, 0xff, 0x00, 0x03, 0xff, 0x00, 0x07, 0xff, 0x80, 0x0f, 0xff,
	0xc0, 0x1f, 0xff, 0xe0, 0x1f, 0xff, 0xe0, 0x3f, 0xff, 0xf0, 0x7e, 0xf9, 0xf8, 0x7c, 0x71, 0xf8,
	0x78, 0x73, 0xf8, 0xfc, 0x63, 0xfc, 0xff, 0xc7, 0xfc, 0xff, 0x8f, 0xfc, 0xff, 0x18, 0xfc, 0x7f,
	0x38, 0x78, 0x7e, 0x38, 0xf8, 0x7e, 0x7d, 0xf8, 0x3f, 0xff, 0xf0, 0x1f, 0xff, 0xe0, 0x0f, 0xff,
	0xc0, 0x03, 0xff, 0x00, 0x00, 0xfc, 0x00
};
#define humidityBigLogoW  22
#define humidityBigLogoH  29

// 'gauge-low', 29x29px
static const unsigned char tachimeterLogo [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x3f, 0xe0, 0x00,
	0x00, 0xff, 0xf8, 0x00, 0x01, 0xe0, 0x3c, 0x00, 0x03, 0x80, 0x0e, 0x00, 0x07, 0x18, 0xc7, 0x00,
	0x0e, 0x18, 0xc3, 0x80, 0x0c, 0x1c, 0x81, 0x80, 0x1c, 0x0c, 0x01, 0xc0, 0x19, 0x8e, 0x0c, 0xc0,
	0x19, 0xcf, 0x1c, 0xc0, 0x18, 0x0f, 0x80, 0xc0, 0x38, 0x0f, 0x80, 0xe0, 0x18, 0x0f, 0x80, 0xc0,
	0x18, 0x07, 0x00, 0xc0, 0x18, 0x00, 0x00, 0xc0, 0x1c, 0x00, 0x01, 0xc0, 0x0c, 0x07, 0x01, 0x80,
	0x0e, 0x3f, 0xe3, 0x80, 0x07, 0xff, 0xff, 0x00, 0x03, 0xff, 0xfe, 0x00, 0x01, 0xff, 0xfc, 0x00,
	0x00, 0xff, 0xf8, 0x00, 0x00, 0x3f, 0xe0, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};
#define tachimeterLogoW  29
#define tachimeterLogoH  29

// 'spot', 22x22px
static const unsigned char spotifyLogo [] PROGMEM = {
	0x01, 0xfe, 0x00, 0x07, 0xff, 0x80, 0x1f, 0xff, 0xe0, 0x3f, 0xff, 0xf0, 0x3f, 0xff, 0xf0, 0x78,
	0x0f, 0xf8, 0x60, 0x00, 0x78, 0xe0, 0x00, 0x1c, 0xef, 0xfc, 0x0c, 0xf8, 0x0f, 0x0c, 0xf0, 0x00,
	0xfc, 0xf0, 0x00, 0x3c, 0xff, 0xfc, 0x3c, 0xf8, 0x07, 0x3c, 0xf0, 0x01, 0xfc, 0x7f, 0xf8, 0x78,
	0x7f, 0xfe, 0xf8, 0x3f, 0xff, 0xf0, 0x3f, 0xff, 0xf0, 0x1f, 0xff, 0xc0, 0x07, 0xff, 0x80, 0x01,
	0xfe, 0x00
};
#define spotifyLogoW  22
#define spotifyLogoH  22

// 'youtube', 22x22px
const unsigned char youtubeLogo [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 
	0xff, 0xe0, 0x1f, 0xff, 0xe0, 0x3f, 0xff, 0xf0, 0x3f, 0xbf, 0xf0, 0x3f, 0x8f, 0xf0, 0x3f, 0x83, 
	0xf0, 0x3f, 0x83, 0xf0, 0x3f, 0x8f, 0xf0, 0x3f, 0xbf, 0xf0, 0x3f, 0xff, 0xf0, 0x1f, 0xff, 0xe0, 
	0x1f, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00
};
#define youtubeLogoW  22
#define youtubeLogoH  22

// 'biohazard', 29x29px
const unsigned char biohazardLogo [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x10, 0x00, 
	0x00, 0xc0, 0x18, 0x00, 0x00, 0x80, 0x08, 0x00, 0x01, 0x80, 0x0c, 0x00, 0x01, 0x80, 0x0c, 0x00, 
	0x01, 0x80, 0x0c, 0x00, 0x01, 0x87, 0x0c, 0x00, 0x01, 0xdf, 0xdc, 0x00, 0x01, 0xf8, 0xfc, 0x00, 
	0x07, 0xf8, 0xff, 0x00, 0x0f, 0xff, 0xff, 0x80, 0x1f, 0xfd, 0xff, 0xc0, 0x38, 0xf8, 0xf8, 0xe0, 
	0x70, 0xd8, 0xd8, 0x70, 0x60, 0xef, 0xb8, 0x30, 0x40, 0x6f, 0xb0, 0x10, 0x40, 0x7f, 0xf0, 0x10, 
	0x00, 0x3f, 0xe0, 0x00, 0x00, 0x0f, 0x80, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x0f, 0x80, 0x00, 
	0x00, 0x1d, 0xc0, 0x00, 0x00, 0x38, 0xe0, 0x00, 0x00, 0xe0, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00
};
#define biohazardLogoW  29
#define biohazardLogoH  29

// 'leaf', 29x29px
const unsigned char leafLogo [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x00, 0xff, 0xc0, 0x00, 0x3f, 0xff, 0xc0, 
	0x00, 0xff, 0xff, 0xc0, 0x03, 0xff, 0xff, 0x80, 0x07, 0xff, 0xff, 0x80, 0x0f, 0xfd, 0xff, 0x00, 
	0x1f, 0xf7, 0xff, 0x00, 0x1f, 0xcf, 0xff, 0x00, 0x1f, 0x9f, 0xfe, 0x00, 0x3f, 0x3f, 0xfc, 0x00, 
	0x3e, 0x7f, 0xfc, 0x00, 0x3c, 0xff, 0xf8, 0x00, 0x1c, 0xff, 0xf0, 0x00, 0x19, 0xff, 0xf0, 0x00, 
	0x09, 0xff, 0xe0, 0x00, 0x03, 0xff, 0xc0, 0x00, 0x03, 0xff, 0x00, 0x00, 0x07, 0xfc, 0x00, 0x00, 
	0x07, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00
};
#define leafLogoW  29
#define leafLogoH  29

// 'butterfly', 29x29px
const unsigned char butterflyLogo [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 
	0x03, 0xc2, 0x00, 0x00, 0x07, 0xe3, 0x00, 0x00, 0x0f, 0xf3, 0x00, 0x00, 0x0f, 0xfb, 0x00, 0x00, 
	0x0f, 0xfb, 0x00, 0x00, 0x0f, 0xfb, 0x07, 0x00, 0x0f, 0xfb, 0x1f, 0xc0, 0x0f, 0xfb, 0x30, 0x40, 
	0x0f, 0xfb, 0x40, 0x00, 0x0f, 0xfb, 0x00, 0x00, 0x07, 0xfa, 0x0f, 0x00, 0x07, 0xf8, 0x7f, 0x80, 
	0x0f, 0xf9, 0xff, 0xc0, 0x1f, 0xff, 0xff, 0xe0, 0x3f, 0xff, 0xff, 0xe0, 0x3f, 0xff, 0xff, 0xe0, 
	0x3f, 0xff, 0xff, 0xe0, 0x3f, 0xdf, 0xff, 0xc0, 0x1f, 0x1f, 0xff, 0x00, 0x00, 0x1f, 0xf8, 0x00, 
	0x00, 0x1f, 0x80, 0x00, 0x00, 0x1f, 0x80, 0x00, 0x00, 0x1f, 0x80, 0x00, 0x00, 0x0f, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00
};
#define butterflyLogoW  29
#define butterflyLogoH  29

// 'skull2', 29x29px
const unsigned char skullLogo [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x3f, 0xe0, 0x00, 
	0x00, 0xff, 0xf8, 0x00, 0x01, 0xff, 0xfc, 0x00, 0x03, 0xff, 0xfe, 0x00, 0x07, 0xff, 0xff, 0x00, 
	0x07, 0xff, 0xff, 0x00, 0x0f, 0xff, 0xff, 0x80, 0x0f, 0xff, 0xff, 0x80, 0x0f, 0xff, 0xff, 0x80, 
	0x0f, 0xff, 0xff, 0x80, 0x0f, 0x9f, 0xcf, 0x80, 0x0f, 0x0f, 0x87, 0x80, 0x0e, 0x0f, 0x83, 0x80, 
	0x0e, 0x0f, 0x83, 0x80, 0x0f, 0x1d, 0xc7, 0x00, 0x07, 0xfd, 0xff, 0x00, 0x03, 0xf8, 0xfe, 0x00, 
	0x03, 0xf8, 0xfe, 0x00, 0x01, 0xff, 0xfc, 0x00, 0x00, 0xff, 0xf8, 0x00, 0x00, 0xe7, 0x38, 0x00, 
	0x00, 0xe7, 0x38, 0x00, 0x00, 0xe7, 0x38, 0x00, 0x00, 0x62, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00
};
#define skullLogoW  29
#define skullLogoH  29

// 'smog (1)', 29x29px
const unsigned char smogLogo [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x80, 0x00, 0x00, 0x1f, 0xc0, 0x00, 
	0x02, 0x3f, 0xe2, 0x00, 0x0f, 0xff, 0xff, 0x80, 0x1f, 0xff, 0xff, 0xc0, 0x3f, 0xff, 0xff, 0xe0, 
	0x3f, 0xff, 0xff, 0xe0, 0x3f, 0xff, 0xff, 0xe0, 0x3f, 0xff, 0xff, 0xe0, 0x1f, 0xff, 0xff, 0xc0, 
	0x0f, 0xff, 0xff, 0x80, 0x07, 0xff, 0xff, 0x00, 0x00, 0xf8, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xf8, 0x00, 0x00, 0xf8, 0xf8, 0x00, 0x00, 0xf8, 0xf8, 0x00, 
	0x00, 0xf8, 0xf8, 0x00, 0x00, 0xf8, 0xf8, 0x00, 0x00, 0xf8, 0xf8, 0x00, 0x00, 0xf8, 0xf8, 0x00, 
	0x00, 0xfd, 0xf8, 0x00, 0x01, 0xfd, 0xfc, 0x00, 0x01, 0xfd, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00
};
#define smogLogoW  29
#define smogLogoH  29

// 'omega', 29x29px
const unsigned char omegaLogo [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xc0, 0x00, 0x00, 0x7f, 0xf0, 0x00, 0x00, 0xf8, 0x78, 0x00, 
	0x01, 0xe0, 0x3c, 0x00, 0x01, 0xc0, 0x1c, 0x00, 0x03, 0x80, 0x0e, 0x00, 0x03, 0x80, 0x0e, 0x00, 
	0x03, 0x80, 0x0e, 0x00, 0x03, 0x80, 0x0e, 0x00, 0x03, 0x80, 0x0e, 0x00, 0x03, 0x80, 0x0e, 0x00, 
	0x01, 0xc0, 0x1c, 0x00, 0x01, 0xc0, 0x1c, 0x00, 0x00, 0xe0, 0x38, 0x00, 0x00, 0x70, 0x70, 0x00, 
	0x03, 0xf8, 0xfe, 0x00, 0x03, 0xf8, 0xfe, 0x00, 0x03, 0xf8, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00
};
#define omegaLogoW  29
#define omegaLogoH  29

// 'water-pump', 30x30px
const unsigned char WATER_PUMP_LOGO [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xf8, 0x00, 0x00, 
	0x03, 0xfc, 0x00, 0x00, 0x03, 0xfc, 0x00, 0x00, 0x07, 0xff, 0xff, 0x80, 0x0f, 0xff, 0xff, 0xc0, 
	0x0f, 0xff, 0xff, 0xc0, 0x0f, 0xff, 0xff, 0xc0, 0x07, 0xff, 0xff, 0xc0, 0x03, 0xfc, 0x07, 0xc0, 
	0x03, 0xfc, 0x07, 0xc0, 0x03, 0xfc, 0x07, 0xc0, 0x03, 0xfc, 0x0f, 0xe0, 0x03, 0xfc, 0x0f, 0xe0, 
	0x03, 0xfc, 0x00, 0x00, 0x03, 0xfc, 0x00, 0x00, 0x03, 0xfc, 0x00, 0x00, 0x03, 0xfc, 0x01, 0x80, 
	0x03, 0xfc, 0x03, 0x80, 0x03, 0xfc, 0x07, 0xc0, 0x03, 0xfc, 0x07, 0xc0, 0x1f, 0xff, 0x83, 0xc0, 
	0x1f, 0xff, 0x83, 0x80, 0x1f, 0xff, 0x80, 0x00, 0x1f, 0xff, 0x80, 0x00, 0x1f, 0xff, 0x80, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#define WATER_PUMP_LOGO_W  30
#define WATER_PUMP_LOGO_H  30

// 'solar-power', 30x30px
const unsigned char SOLAR_STATION_LOGO [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xfb, 0x00, 0x00, 
	0x1f, 0xfb, 0xc0, 0x00, 0x1f, 0xfb, 0x80, 0x00, 0x1f, 0xfa, 0x00, 0x00, 0x1f, 0xf0, 0x00, 0x00, 
	0x1f, 0xf0, 0x00, 0x00, 0x1f, 0xe0, 0x00, 0x00, 0x1f, 0xc0, 0x01, 0x00, 0x1f, 0x8c, 0x03, 0x00, 
	0x1e, 0x1c, 0x03, 0x00, 0x00, 0x1c, 0x07, 0x00, 0x1e, 0x00, 0x07, 0x00, 0x1c, 0x00, 0x0f, 0x00, 
	0x0c, 0x00, 0x1f, 0xe0, 0x08, 0x00, 0x1f, 0xe0, 0x00, 0x00, 0x3f, 0xc0, 0x00, 0x00, 0x3f, 0xc0, 
	0x00, 0x00, 0x07, 0x80, 0x00, 0x00, 0x07, 0x80, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x06, 0x00, 
	0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#define SOLAR_STATION_LOGO_W  30
#define SOLAR_STATION_LOGO_H  30

// 'cloud-upload', 30x30px
const unsigned char UPLOAD_LOGO [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xc0, 0x00, 0x00, 0x3f, 0xf0, 0x00, 0x00, 0x7f, 0xf8, 0x00, 
	0x00, 0xff, 0xfc, 0x00, 0x01, 0xff, 0xfe, 0x00, 0x07, 0xfc, 0xfe, 0x00, 0x1f, 0xf8, 0x7f, 0x00, 
	0x3f, 0xf0, 0x3f, 0x00, 0x7f, 0xe0, 0x1f, 0xe0, 0x7f, 0xc0, 0x0f, 0xf0, 0xff, 0x80, 0x07, 0xf8, 
	0xff, 0xf0, 0x3f, 0xfc, 0xff, 0xf0, 0x3f, 0xfc, 0xff, 0xf0, 0x3f, 0xfc, 0xff, 0xf0, 0x3f, 0xfc, 
	0x7f, 0xf0, 0x3f, 0xfc, 0x7f, 0xff, 0xff, 0xf8, 0x3f, 0xff, 0xff, 0xf8, 0x1f, 0xff, 0xff, 0xf0, 
	0x07, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#define UPLOAD_LOGO_W  30
#define UPLOAD_LOGO_H  30

/********************************** FUNCTION DECLARATION (NEEDED BY PLATFORMIO WHILE COMPILING CPP FILES) *****************************************/
// Bootstrap functions
void callback(char* topic, byte* payload, unsigned int length);
void manageDisconnections();
void manageQueueSubscription();
void manageHardwareButton();
// Project specific functions
bool processSmartostatSensorJson(StaticJsonDocument<BUFFER_SIZE> json);
bool processUpsStateJson(StaticJsonDocument<BUFFER_SIZE> json);
bool processSmartostatAcJson(StaticJsonDocument<BUFFER_SIZE> json);
bool processSmartostatClimateJson(StaticJsonDocument<BUFFER_SIZE> json);
bool processSpotifyStateJson(StaticJsonDocument<BUFFER_SIZE> json);
bool processSmartostatPirState(StaticJsonDocument<BUFFER_SIZE> json);
bool processSmartoledCmnd(StaticJsonDocument<BUFFER_SIZE> json);
bool processFurnancedCmnd(StaticJsonDocument<BUFFER_SIZE> json);
bool processSolarStationPowerState(StaticJsonDocument<BUFFER_SIZE> json);
bool processSolarStationWaterPump(StaticJsonDocument<BUFFER_SIZE> json);
bool processSolarStationState(StaticJsonDocument<BUFFER_SIZE> json);
void drawHeader();
void drawRoundRect();
void drawRoundRect();
void drawFooterThermostat();
void drawFooterSolarStation();
void manageFooter();
void drawUpsFooter();
void drawCenterScreenLogo(bool &triggerBool, const unsigned char *logo, const int logoW, const int logoH, const int delayInt);
void drawCenterScreenLogo(const unsigned char* logo, const int logoW, const int logoH);
void drawWPRemainingSeconds(const unsigned char *logo, const int logoW, const int logoH);
void drawSolarStationTrigger(const unsigned char* logo, const int logoW, const int logoH);
void sendPowerState();
void quickPress();
void longPressRelease();
void veryLongPressRelease();
void commandButtonRelease();
void quickPressRelease();
void readConfigFromSPIFFS();
void writeConfigToSPIFFS();
void resetMinMaxValues();
void touchButtonManagement(int pinvalue);
void sendACCommandState();
void sendClimateState(String mode);  
void sendFurnanceCommandState();
void cleanSpotifyInfo();
#ifdef TARGET_SMARTOSTAT
  void sendSmartostatRebootState(String onOff);
  void sendSmartostatRebootCmnd();
  void sendPirState();
  void sendSensorState();
  void pirManagement();
  void releManagement();
  void acManagement();
  void sendFurnanceState();
  void sendACState();  
  bool processIrOnOffCmnd(StaticJsonDocument<BUFFER_SIZE> json);
  bool processIrSendCmnd(StaticJsonDocument<BUFFER_SIZE> json);
  bool processSmartostatRebootCmnd(StaticJsonDocument<BUFFER_SIZE> json);
  void getGasReference();
  String calculateIAQ(int score); 
  int getHumidityScore();
  int getGasScore();
#endif
#ifdef TARGET_SMARTOLED
  void sendSmartoledRebootState(String onOff);
  void sendSmartoledRebootCmnd();
  bool processSmartostatFurnanceState(StaticJsonDocument<BUFFER_SIZE> json);
  bool processACState(StaticJsonDocument<BUFFER_SIZE> json);
  bool processSmartoledRebootCmnd(StaticJsonDocument<BUFFER_SIZE> json);
#endif
