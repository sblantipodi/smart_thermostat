#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#ifdef TARGET_SMARTOSTAT_OLED
  #include <Adafruit_Sensor.h>
  #include <Adafruit_BME680.h>
  #include <IRremoteESP8266.h>
  #include <IRsend.h>
  #include <ir_Samsung.h>
#endif

/************ SMARTOSTAT VERSION ******************/
const String SMARTOSTAT_VERSION = "5.6.2";

/************ WIFI and MQTT Info ******************/
const char* ssid = "XXXXXX";
const char* password = "XXXXXX";
const char* mqtt_server = "192.168.1.XX";
const char* mqtt_username = "XXXXXX";
const char* mqtt_password = "XXXXXX";
const int mqtt_port = XX;
// DNS address for the shield:
IPAddress mydns(192, 168, 1, XX);
// GATEWAY address for the shield:
IPAddress mygateway(192, 168, 1, XX);

/**************************** OTA **************************************************/
#ifdef TARGET_SMARTOSTAT_OLED
  #define SENSORNAME "smartostat_oled"
  int OTAport = XX;
  // IP address for the shield:
  IPAddress arduinoip_smartostat(192, 168, 1, XX);
#endif 
#ifdef TARGET_SMARTOLED
  #define SENSORNAME "smartoled"
  int OTAport = XX;
  // IP address for the shield:
  IPAddress arduinoip(192, 168, 1, XX); 
#endif 
#define OTApassword "XXXXXX"

/**************************** PIN DEFINITIONS **************************************************/
#define OLED_RESET LED_BUILTIN // Pin used for integrated D1 Mini blue LED
#ifdef TARGET_SMARTOSTAT_OLED
  #define OLED_BUTTON_PIN 12 // D6 Pin, 5V power, for capactitive touch sensor. When Sig Output is high, touch sensor is being 
#else
  #define OLED_BUTTON_PIN 14 // D5 Pin, 5V power, for capactitive touch sensor. When Sig Output is high, touch sensor is being 
#endif
#define SMARTOSTAT_BUTTON_PIN 15 // D8 pin, 5V power, for touch button used to start stop the furnance 
#ifdef TARGET_SMARTOSTAT_OLED
  #define SR501_PIR_PIN 16 // D0 Pin, 5V power, for SR501 PIR sensor
  #define RELE_PIN 14 // D5 Pin, 5V power, for relè //TODO configurare releè pin
  Adafruit_BME680 boschBME680; // D2 pin SDA, D1 pin SCL, 3.3V power for BME680 sensor, sensor address I2C 0x76
  const uint16_t kIrLed = D3; // GPIOX for IRsender
  IRSamsungAc acir(kIrLed);  
#endif

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins) // Address 0x3C for 128x64pixel
// D2 pin SDA, D1 pin SCL, 5V power 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); 

// Serial rate for debug
#define serialRate 115200

/************* MQTT TOPICS **************************/
const char* smartostat_sensor_state_topic = "tele/smartostat/SENSOR";
const char* smartostat_state_topic = "tele/smartostat/STATE";
const char* smartostat_climate_state_topic = "stat/smartostat/CLIMATE";
const char* smartostatac_cmd_topic = "cmnd/smartostatac/CLIMATE";
const char* smartostat_furnance_state_topic = "stat/smartostat/POWER1";
const char* smartostat_pir_state_topic = "stat/smartostat/POWER2";
const char* spotify_state_topic = "stat/spotify/info";
const char* smartostat_furnance_cmnd_topic = "cmnd/smartostat/POWER1";
const char* smartostatac_stat_irsend = "stat/smartostatac/IRsend";
const char* smartostatac_cmnd_irsend = "cmnd/smartostatac/IRsendCmnd";
const char* smartostat_cmnd_climate_heat_state = "cmnd/smartostat/climateHeatState";
const char* smartostat_cmnd_climate_cool_state = "cmnd/smartostat/climateCoolState";
#ifdef TARGET_SMARTOSTAT_OLED
  const char* smartoled_cmnd_topic = "cmnd/smartostat_oled/POWER3";
  const char* smartoled_state_topic = "stat/smartostat_oled/POWER3";
  const char* smartoled_info_topic = "stat/smartostat_oled/INFO";
  const char* smartostatac_cmnd_irsendState = "cmnd/smartostatac/IRsend";
  const char* smartostat_stat_reboot = "stat/smartostat/reboot";
  const char* smartostat_cmnd_reboot = "cmnd/smartostat/reboot";
#endif
#ifdef TARGET_SMARTOLED
  const char* smartoled_cmnd_topic = "cmnd/smartoled/POWER3";
  const char* smartoled_state_topic = "stat/smartoled/POWER3";
  const char* smartoled_info_topic = "stat/smartoled/INFO";
  const char* smartoled_stat_reboot = "stat/smartoled/reboot";
  const char* smartoled_cmnd_reboot = "cmnd/smartoled/reboot";
#endif 

// Display state
bool stateOn = true;

// LED_BUILTIN vars
unsigned long previousMillis = 0;    // will store last time LED was updated
const long interval = 200;           // interval at which to blink (milliseconds)
bool ledTriggered = false;
const int blinkTimes = 6;            // 6 equals to 3 blink on and 3 off
int blinkCounter = 0;

// Button state variables
byte buttonState = 0;
byte  lastReading = 0;
unsigned long onTime = 0;
bool longPress = false;
bool veryLongPress = false;

// Humidity Threshold
const int HUMIDITYTHRESHOLD = 65;

const char* on_cmd = "ON";
const char* off_cmd = "OFF";

// Total Number of pages
const int numPages = 8;
String temperature = "OFF";
float minTemperature = 99;
float maxTemperature = 0.0;
String humidity = "OFF";
float minHumidity = 99;
float maxHumidity = 0.0;
String pressure = "OFF";
float minPressure = 2000;
float maxPressure = 0.0;
String gasResistance = "OFF";
float minGasResistance = 2000;
float maxGasResistance = 0.0;
String IAQ = off_cmd; // indoor air quality
float minIAQ = 2000;
float maxIAQ = 0.0;
String lastBoot = " ";
String lastMQTTConnection = "OFF";
String lastWIFiConnection = "OFF";
String furnance = "OFF";
String ac = "OFF";
String pir = "OFF";
String target_temperature = "OFF";
String hvac_action = "OFF";
String fan = "OFF";
const String COOL = "cooling";
const String HEAT = "heating";
const String IDLE = "idle";
const int HEAT_COOL_THRESHOLD = 25;
String away_mode = "OFF";
String timedate = "OFF";
String date = "OFF";
String currentime = "OFF";
String alarm = "OFF";
bool furnanceTriggered = false;
bool acTriggered = false;
const int delay_3000 = 3000;
const int delay_2000 = 2000;
const int delay_4000 = 4000;
const int delay_1500 = 1500;
int currentPage = 0;
bool showHaSplashScreen = true;
String haVersion = "";
String hours = "";
String minutes = "";
bool pressed = false;
String spotifySource = "";
String volumeLevel = "";
String mediaPosition = "";
String mediaDuration = "";
String mediaArtist = "";
String mediaTitle = "";
String spotifyActivity = "";
int offset = 160;
int offsetAuthor = 130;
int yoffset = 150;
bool lastPageScrollTriggered = false;
bool screenSaverTriggered = false;
// variable used for faster delay instead of arduino delay(), this custom delay prevent a lot of problem and memory leak
const int tenSecondsPeriod = 10000;
unsigned long timeNowStatus = 0;
// five minutes
const int fiveMinutesPeriod = 300000;
unsigned long timeNowGoHomeAfterFiveMinutes = 0;
unsigned int lastButtonPressed = 0;
#define MAX_RECONNECT 500
unsigned int delayTime = 20;

#ifdef TARGET_SMARTOSTAT_OLED
  // PIR variables
  long unsigned int highIn;
 
  unsigned int readOnceEveryNTimess = 0;
  const char* lastPirState = off_cmd;
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

/****************************************FOR JSON***************************************/
const int BUFFER_SIZE = JSON_OBJECT_SIZE(20);
// edit this variable on C:\Users\sblantipodi\Documents\Arduino\libraries\PubSubClient\src\PubSubClient.h and set it to 512
#define MQTT_MAX_PACKET_SIZE 512


WiFiClient espClient;
PubSubClient client(espClient);

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

// 'home-assistant_big', 44x44px
static const unsigned char habigLogo [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xc0, 0x00, 0x00,
	0x00, 0x00, 0x7f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x01, 0xff, 0xf8,
	0x00, 0x00, 0x00, 0x03, 0xf0, 0xfd, 0xe0, 0x00, 0x00, 0x07, 0xe0, 0x7f, 0xe0, 0x00, 0x00, 0x0f,
	0xcf, 0x3f, 0xe0, 0x00, 0x00, 0x1f, 0xcf, 0x3f, 0xe0, 0x00, 0x00, 0x3f, 0xe6, 0x7f, 0xe0, 0x00,
	0x00, 0x7f, 0xe0, 0x7f, 0xe0, 0x00, 0x00, 0xff, 0xf0, 0xff, 0xf0, 0x00, 0x01, 0xff, 0xf0, 0xff,
	0xf8, 0x00, 0x03, 0xff, 0xf0, 0xff, 0xfc, 0x00, 0x07, 0xf0, 0xf0, 0xf0, 0xfe, 0x00, 0x0f, 0xe0,
	0x70, 0xe0, 0x7f, 0x00, 0x1f, 0xce, 0x70, 0xe7, 0x3f, 0x80, 0x1f, 0xcf, 0x70, 0xef, 0x3f, 0x80,
	0x01, 0xce, 0x70, 0xe7, 0x38, 0x00, 0x01, 0xe0, 0x30, 0xc0, 0x78, 0x00, 0x01, 0xf0, 0x10, 0x80,
	0xf8, 0x00, 0x01, 0xff, 0x00, 0x0f, 0xf8, 0x00, 0x01, 0xff, 0x80, 0x0f, 0xf8, 0x00, 0x01, 0xff,
	0xc0, 0x1f, 0xf8, 0x00, 0x01, 0xff, 0xe0, 0x3f, 0xf8, 0x00, 0x01, 0xff, 0xf0, 0x7f, 0xf8, 0x00,
	0x01, 0xff, 0xf0, 0xff, 0xf8, 0x00, 0x01, 0xff, 0xf0, 0xff, 0xf8, 0x00, 0x01, 0xff, 0xf0, 0xff,
	0xf8, 0x00, 0x01, 0xff, 0xf0, 0xff, 0xf8, 0x00, 0x01, 0xff, 0xf0, 0xff, 0xf8, 0x00, 0x01, 0xff,
	0xf0, 0xff, 0xf8, 0x00, 0x01, 0xff, 0xf0, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#define habigLogoW  44
#define habigLogoH  44

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

// 'arduino', 45x31px
const unsigned char arduinoLogo [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x01, 0xe0, 0x00, 0x01, 0xff, 0x80, 0x0f, 
	0xfc, 0x40, 0x07, 0xff, 0xe0, 0x3f, 0xff, 0x00, 0x0f, 0xff, 0xf0, 0x7f, 0xff, 0x80, 0x1f, 0x01, 
	0xf8, 0xfc, 0x07, 0xc0, 0x1e, 0x00, 0x7d, 0xf0, 0x03, 0xc0, 0x3c, 0x00, 0x3d, 0xe0, 0x01, 0xe0, 
	0x38, 0x00, 0x1f, 0xc0, 0xc0, 0xe0, 0x78, 0x00, 0x1f, 0xc0, 0xc0, 0xf0, 0x78, 0x7e, 0x0f, 0x83, 
	0xf0, 0xf0, 0x78, 0x7e, 0x0f, 0x83, 0xf0, 0xf0, 0x78, 0x00, 0x0f, 0x80, 0xc0, 0xf0, 0x38, 0x00, 
	0x1f, 0xc0, 0xc0, 0xe0, 0x3c, 0x00, 0x3d, 0xe0, 0x01, 0xe0, 0x3e, 0x00, 0x7d, 0xf0, 0x03, 0xe0, 
	0x1f, 0x00, 0xf8, 0xf8, 0x07, 0xc0, 0x0f, 0xe7, 0xf0, 0x7f, 0x3f, 0x80, 0x07, 0xff, 0xe0, 0x3f, 
	0xff, 0x00, 0x03, 0xff, 0xc0, 0x1f, 0xfe, 0x00, 0x00, 0xff, 0x00, 0x07, 0xf8, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x7b, 0xec, 0x9e, 0xc9, 0xe0, 
	0x1c, 0x4b, 0x2c, 0x8c, 0xeb, 0x20, 0x16, 0x5b, 0x3c, 0x8c, 0xea, 0x30, 0x16, 0x73, 0x3c, 0x8c, 
	0xfa, 0x30, 0x1e, 0x5b, 0x2c, 0x8c, 0xdb, 0x30, 0x22, 0x4b, 0x6c, 0x8c, 0xd9, 0x60, 0x23, 0x4f, 
	0x83, 0x1e, 0xc8, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#define arduinoLogoW  45
#define arduinoLogoH  31

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


/********************************** FUNCTION DECLARATION (NEEDED BY VSCODE WHILE COMPILING CPP FILES) *****************************************/
bool processSmartostatSensorJson(char *message);
bool processSmartostatAcJson(char *message);
bool processSmartostatClimateJson(char *message);
bool processSpotifyStateJson(char *message);
bool processSmartostatPirState(char *message);
bool processSmartoledCmnd(char *message);
bool processFurnancedCmnd(char *message);
void printLastPage();
void drawHeader();
void drawRoundRect();
void drawRoundRect();
void drawFooter();
void drawCenterScreenLogo(bool &triggerBool, const unsigned char *logo, const int logoW, const int logoH, const int delayInt);
void drawScreenSaver();
void sendPowerState();
void quickPress();
void longPressRelease();
void veryLongPressRelease();
void commandButtonRelease();
void quickPressRelease();
void readConfigFromSPIFFS();
void writeConfigToSPIFFS();
void resetMinMaxValues();
int getQuality();
void nonBlokingBlink();
void setDateTime(const char* timeConst);
void touchButtonManagement(int pinvalue);
void sendACCommandState();
void sendClimateState(String mode);  
void sendFurnanceCommandState();
#ifdef TARGET_SMARTOSTAT_OLED
  void sendSmartostatRebootState(const char* onOff);
  void sendSmartostatRebootCmnd();
  void sendPirState();
  void sendSensorState();
  void pirManagement();
  void releManagement();
  void acManagement();
  void sendFurnanceState();
  void sendACState();  
  void manageSmartostatButton();
  bool processIrOnOffCmnd(char *message);
  bool processIrSendCmnd(char *message);
  bool processSmartostatRebootCmnd(char *message);
  void getGasReference();
  String calculateIAQ(int score); 
  int getHumidityScore();
  int getGasScore();
#endif
#ifdef TARGET_SMARTOLED
  void sendSmartoledRebootState(const char* onOff);
  void sendSmartoledRebootCmnd();
  bool processSmartostatFurnanceState(char *message);
  bool processACState(char *message);
  bool processSmartoledRebootCmnd(char *message);
#endif
