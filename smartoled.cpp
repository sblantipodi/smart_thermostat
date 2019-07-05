/*
 * DPsoftware Smart Thermostat
 * by Davide Perini
 * MIT license
 */

#include <FS.h> //this needs to be first, or it all crashes and burns...
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



/************ WIFI and MQTT Info ******************/
const char* ssid = "XXXXXX";
const char* password = "XXXXXX";
const char* mqtt_server = "192.168.1.XX";
const char* mqtt_username = "XXXXXX";
const char* mqtt_password = "XXXXXX";
const int mqtt_port = 1883;
// DNS address for the shield:
IPAddress mydns(192, 168, 1, 1);
// GATEWAY address for the shield:
IPAddress mygateway(192, 168, 1, 1);

/**************************** OTA **************************************************/
#ifdef TARGET_SMARTOSTAT_OLED
  #define SENSORNAME "smartostat_oled"
  int OTAport = 8268;
  // IP address for the shield:
  IPAddress arduinoip_smartostat(192, 168, 1, 10);
#endif 
#ifdef TARGET_SMARTOLED
  #define SENSORNAME "smartoled"
  int OTAport = 8888;
  // IP address for the shield:
  IPAddress arduinoip(192, 168, 1, 9); 
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
#ifdef TARGET_SMARTOSTAT_OLED
  const char* smartoled_cmnd_topic = "cmnd/smartostat_oled/POWER3";
  const char* smartoled_state_topic = "stat/smartostat_oled/POWER3";
  const char* smartoled_info_topic = "stat/smartostat_oled/INFO";
  const char* smartostatac_cmnd_irsendState = "cmnd/smartostatac/IRsend";
  const char* smartostatac_cmnd_irsend = "cmnd/smartostatac/IRsendCmnd";
  const char* smartostat_cmnd_climate_heat_state = "cmnd/smartostat/climateHeatState";
  const char* smartostat_cmnd_climate_cool_state = "cmnd/smartostat/climateCoolState";
#endif
#ifdef TARGET_SMARTOLED
  const char* smartoled_cmnd_topic = "cmnd/smartoled/POWER3";
  const char* smartoled_state_topic = "stat/smartoled/POWER3";
  const char* smartoled_info_topic = "stat/smartoled/INFO";
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

// Humidity Threshold
int humidityThreshold = 65;

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
String operation_mode = "OFF";
const String COOL = "cool";
const String HEAT = "heat";
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
String mediaArtist = "";
String mediaTitle = "";
String spotifyActivity = "";
int offset = 160;
int offsetAuthor = 130;
int yoffset = 150;
bool lastPageScrollTriggered = false;
// variable used for faster delay instead of arduino delay(), this custom delay prevent a lot of problem and memory leak
const int tenSecondsPeriod = 10000;
unsigned long timeNowStatus = 0;
// five minutes
const int fiveMinutesPeriod = 300000;
unsigned long timeNowGoHomeAfterFiveMinutes = 0;
unsigned int lastButtonPressed = 0;
#define MAX_RECONNECT 500

#ifdef TARGET_SMARTOSTAT_OLED
  // PIR variables
  long unsigned int highIn;
  // only button can force furnance state to ON even when wifi/mqtt is disconnected, the force state is resetted to OFF even by MQTT topic
  boolean forceFurnanceOn = false;
  boolean forceACOn = false;
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
void sendPowerState();
void quickPress();
void longPressRelease();
void quickPressRelease();
void readConfigFromSPIFFS();
void writeConfigToSPIFFS();
void resetMinMaxValues();
int getQuality();
void nonBlokingBlink();
void setDateTime(const char* timeConst);
void touchButtonManagement(int pinvalue);
#ifdef TARGET_SMARTOSTAT_OLED
  void sendPirState();
  void sendSensorState();
  void pirManagement();
  void releManagement();
  void acManagement();
  void sendFurnanceState();
  void sendACState();
  void sendClimateState(String mode);  
  void manageSmartostatButton();
  bool processIrOnOffCmnd(char *message);
  bool processIrSendCmnd(char *message);
  void getGasReference();
  String calculateIAQ(int score); 
  int getHumidityScore();
  int getGasScore();
#endif
#ifdef TARGET_SMARTOLED
  bool processSmartostatFurnanceState(char *message);
  bool processACState(char *message);
#endif


/********************************** START CALLBACK*****************************************/
void callback(char* topic, byte* payload, unsigned int length) {
  // Serial.print(F("Message arrived from [");
  // Serial.print(topic);
  // Serial.println(F("] ");

  char message[length + 1];
  for (unsigned int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  //Serial.println(message);

  if(strcmp(topic, smartostat_sensor_state_topic) == 0) {
    if (!processSmartostatSensorJson(message)) {
      return;
    }
  }
  #ifdef TARGET_SMARTOLED
    if(strcmp(topic, smartostatac_cmd_topic) == 0) {
      if (!processSmartostatAcJson(message)) {
        return;
      }
    }
    if(strcmp(topic, smartostat_furnance_state_topic) == 0) {
      if (!processSmartostatFurnanceState(message)) {
        return;
      }
    }
    if(strcmp(topic, smartostatac_stat_irsend) == 0) {
      if (!processACState(message)) {
        return;
      }
    }
  #endif  
  if(strcmp(topic, smartostat_climate_state_topic) == 0) {
    if (!processSmartostatClimateJson(message)) {
      return;
    }
  }
  if(strcmp(topic, smartostat_state_topic) == 0) {
    if (!processSmartostatSensorJson(message)) {
      return;
    }
  }
  if(strcmp(topic, spotify_state_topic) == 0) {
    if (!processSpotifyStateJson(message)) {
      return;
    }
  }  
  if(strcmp(topic, smartostat_pir_state_topic) == 0) {
    if (!processSmartostatPirState(message)) {
      return;
    }
  }
  if(strcmp(topic, smartoled_cmnd_topic) == 0) {
    if (!processSmartoledCmnd(message)) {
      return;
    }
  }
  if(strcmp(topic, smartostat_furnance_cmnd_topic) == 0) {
    if (!processFurnancedCmnd(message)) {
      return;
    }
  }
  #ifdef TARGET_SMARTOSTAT_OLED
    if(strcmp(topic, smartostatac_cmnd_irsendState) == 0) {
      if (!processIrOnOffCmnd(message)) {
        return;
      }
    }
    if(strcmp(topic, smartostatac_cmnd_irsend) == 0) {
      if (!processIrSendCmnd(message)) {
        return;
      }
    }
  #endif
}

void draw() {

  // pagina 0,1,2,3,4,5 sono fisse e sono temp, humidita, pressione, min-maximum, sporitfy
  // lastPage contiene le info su smartoled
  display.clearDisplay();
  // currentPage = 7;

  if (currentPage == 7) {
    if (spotifyActivity != "playing") {
      display.clearDisplay();
      currentPage = numPages;
      printLastPage();
      return;
    }
  }

  if (currentPage != numPages && currentPage != 7) {
    drawHeader();
  }

  // Draw Images
  if (alarm == "armed_away" || alarm == "pending" || alarm == "triggered") {
    display.drawBitmap(0, 10, shieldLogo, shieldLogoW, shieldLogoH, 1);
  } else if (away_mode == "off" && currentPage != numPages) {
    display.drawBitmap(0, 10, haSmallLogo, haSmallLogoW, haSmallLogoH, 1);
  }

  if (humidity != "OFF" && humidity.toFloat() >= humidityThreshold) {
    currentPage = 0;
    display.drawBitmap(14, 18, humidityLogo, humidityLogoW, humidityLogoH, 1);
  } else if (furnance == on_cmd && currentPage == 0) {
    drawRoundRect();
    display.drawBitmap(14, 18, fireLogo, fireLogoW, fireLogoH, 1);
  } else if (ac == on_cmd && currentPage == 0) {
    drawRoundRect();
    display.drawBitmap(14, 18, snowLogo, snowLogoW, snowLogoH, 1);
  } else if (operation_mode == HEAT && currentPage == 0) {
    display.drawBitmap(9, 18, tempLogo, tempLogoW, tempLogoH, 1);
  } else if (operation_mode == COOL && currentPage == 0) {
    display.drawBitmap(9, 18, coolLogo, coolLogoW, coolLogoH, 1);
  } else if (currentPage == 1) {
    display.drawBitmap(15, 18, humidityBigLogo, humidityBigLogoW, humidityBigLogoH, 1);
  } else if (currentPage == 2) {
    display.drawBitmap(5, 18, tachimeterLogo, tachimeterLogoW, tachimeterLogoH, 1);
  } else if (currentPage == 3) {
    display.drawBitmap(5, 18, omegaLogo, omegaLogoW, omegaLogoH, 1);
  } else if (currentPage == 4) {
    if      (IAQ.toInt() >= 301)                        display.drawBitmap(5, 18, biohazardLogo, biohazardLogoW, biohazardLogoH, 1);
    else if (IAQ.toInt() >= 201 && IAQ.toInt() <= 300 ) display.drawBitmap(5, 18, skullLogo, skullLogoW, skullLogoH, 1);
    else if (IAQ.toInt() >= 151 && IAQ.toInt() <= 200 ) display.drawBitmap(5, 18, smogLogo, smogLogoW, smogLogoH, 1);
    else if (IAQ.toInt() >=  51 && IAQ.toInt() <= 150 ) display.drawBitmap(5, 18, leafLogo, leafLogoW, leafLogoH, 1);
    else if (IAQ.toInt() >=  00 && IAQ.toInt() <=  50 ) display.drawBitmap(5, 18, butterflyLogo, butterflyLogoW, butterflyLogoH, 1);    
  } else if (currentPage == 5) {
    if (operation_mode == HEAT) {
      display.drawBitmap(((display.width()/3)/2)-(tempLogoW/2), 15, tempLogo, tempLogoW, tempLogoH, 1);
    } else if (operation_mode == COOL) {
      display.drawBitmap(((display.width()/3)/2)-(coolLogoW/2), 15, coolLogo, coolLogoW, coolLogoH, 1);
    } else {
      display.drawBitmap(((display.width()/3)/2)-(offLogoW/2), 15, offLogo, offLogoW, offLogoH, 1);
    }
    display.drawBitmap((display.width()/2)-(humidityBigLogoW/2), 15, humidityBigLogo, humidityBigLogoW, humidityBigLogoH, 1);
    display.drawBitmap(display.width()-((display.width()/3)/2)-(tachimeterLogoW/2), 15, tachimeterLogo, tachimeterLogoW, tachimeterLogoH, 1);
  } else if (currentPage == 6) {
    int dispDivide = display.width()/5;
    display.drawBitmap((dispDivide), 15, omegaLogo, omegaLogoW, omegaLogoH, 1);
    if      (IAQ.toInt() >= 301)                        display.drawBitmap((dispDivide*3), 15, biohazardLogo, biohazardLogoW, biohazardLogoH, 1);
    else if (IAQ.toInt() >= 201 && IAQ.toInt() <= 300 ) display.drawBitmap((dispDivide*3), 15, skullLogo, skullLogoW, skullLogoH, 1);
    else if (IAQ.toInt() >= 151 && IAQ.toInt() <= 200 ) display.drawBitmap((dispDivide*3), 15, smogLogo, smogLogoW, smogLogoH, 1);
    else if (IAQ.toInt() >=  51 && IAQ.toInt() <= 150 ) display.drawBitmap((dispDivide*3), 15, leafLogo, leafLogoW, leafLogoH, 1);
    else if (IAQ.toInt() >=  00 && IAQ.toInt() <=  50 ) display.drawBitmap((dispDivide*3), 15, butterflyLogo, butterflyLogoW, butterflyLogoH, 1);
  } else if (currentPage == 7) {
    // do nothing here image is shown in the text area
  } else if (currentPage == numPages) {
    // display.drawBitmap((display.width()-habigLogoW)-1, 0, habigLogo, habigLogoW, habigLogoH, 1);
  } else {
    currentPage = 0;
    display.drawBitmap(10, 18, offLogo, offLogoW, offLogoH, 1);
  }

  if (currentPage != 5 && currentPage != 6 && currentPage != 7 && currentPage != numPages) {
    drawFooter();
  }

  // Draw Text
  display.setTextSize(2);

  if (humidity != "OFF" && humidity.toFloat() >= humidityThreshold) {
    currentPage = 0;
    display.setCursor(55,14);
    display.print(humidity); display.println(F("%"));
    display.setCursor(55,35);
    display.print(temperature); display.print(F("C"));
  } else if (currentPage == 0) {
    display.setCursor(55,25);
    display.print(temperature); display.print(F("C"));
  } else if (currentPage == 1) {
    display.setCursor(55,25);
    display.print(humidity); display.print(F("%"));
  } else if (currentPage == 2) {
    display.setCursor(35,25);
    display.print(pressure);
    display.setTextSize(1);
    display.print(F("hPa"));
   } else if (currentPage == 3) {
    display.setCursor(35,25);
    display.print(gasResistance);
    display.setTextSize(1);
    display.print(F("KOhms"));
  } else if (currentPage == 4) {
    display.setCursor(40,25);
    display.print(IAQ);
    display.setTextSize(1);
    display.print(F("IAQ"));
  } else if (currentPage == 5) {
    display.setTextSize(1);

    display.setCursor(8,47);
    display.print(minTemperature, 1); display.println(F("C"));
    display.setCursor(8,57);
    display.print(maxTemperature, 1); display.println(F("C"));

    display.setCursor(50,47);
    display.print(minHumidity, 1); display.println(F("%"));
    display.setCursor(50,57);
    display.print(maxHumidity, 1); display.println(F("%"));

    display.setCursor(90,47);
    display.print(minPressure, 1);
    display.setCursor(90,57);
    display.print(maxPressure, 1);

  } else if (currentPage == 6) {
    display.setTextSize(1);

    display.setCursor(10,47);
    display.print(minGasResistance, 1); display.println(F("KOhms"));
    display.setCursor(10,57);
    display.print(maxGasResistance, 1); display.println(F("KOhms"));

    display.setCursor(75,47);
    display.print(minIAQ, 1); display.println(F("IAQ"));
    display.setCursor(75,57);
    display.print(maxIAQ, 1); display.println(F("IAQ"));

  } else if (currentPage == 7) {
    if (spotifyActivity == "playing") {
      display.clearDisplay();
      // display.fillTriangle(2, 8, 7, 3, 12, 8, WHITE);
      display.fillTriangle(0, 0, 4, 4, 0, 8, WHITE);
      display.drawBitmap((display.width()/2)-(spotifyLogoW/2), 0, spotifyLogo, spotifyLogoW, spotifyLogoH, 1);
      display.setTextSize(2);
      display.setTextWrap(false);

      // 12 is the text width
      int titleLen = mediaTitle.length()*12;
      if (-titleLen > offset) {
        offset = 160;
      } else {
        offset -= 2;
      }
      display.setCursor(offset,spotifyLogoW+5);
      display.println(mediaTitle);

      // 6 is the text width
      int authorLen = mediaArtist.length()*6;
      if (authorLen > 128) {
        if (-authorLen > offsetAuthor) {
          offsetAuthor = 130;
        } else {
          offsetAuthor -= 1;
        }
      } else {
        offsetAuthor = 0;
      }
      display.setTextSize(1);
      display.setCursor(offsetAuthor,47);
      display.println(mediaArtist);

      // draw volum bar
      float roundedVolumeLevel = volumeLevel.toFloat();
      int volume = (roundedVolumeLevel > 0.99) ? display.width() : ((roundedVolumeLevel*100)*1.28);
      display.drawRect(0,(display.height()-4),display.width(),4, WHITE);
      display.fillRect(0,(display.height()-4),volume,4, WHITE);
    }
  } else if (currentPage == numPages) {
    printLastPage();
  }
  display.setTextWrap(true);

  if (pir == on_cmd && currentPage != numPages) {
    // display.fillCircle(124, 13, 2, WHITE);
    if (currentPage == 7) {
      display.drawBitmap(117, 0, runLogo, runLogoW, runLogoH, 1);
    } else {
      display.drawBitmap(117, 12, runLogo, runLogoW, runLogoH, 1);
    }
  }

  if (furnanceTriggered == true) {
    drawCenterScreenLogo(furnanceTriggered, fireLogo, fireLogoW, fireLogoH, delay_4000);
  }

  if (acTriggered == true) {
    drawCenterScreenLogo(acTriggered, snowLogo, snowLogoW, snowLogoH, delay_4000);
  }

  if (showHaSplashScreen) {
    drawCenterScreenLogo(showHaSplashScreen, habigLogo, habigLogoW, habigLogoH, delay_4000);
  }

  if (temperature != off_cmd) {
    display.display();
  }

  /*Serial.print(F("Temp: "); Serial.print(temperature); Serial.println(F("°C");
  Serial.print(F("Humidity: "); Serial.print(humidity); Serial.println(F("%");
  Serial.print(F("Pressure: "); Serial.print(pressure); Serial.println(F("hPa");

  Serial.print(F("Caldaia: "); Serial.println(furnance);
  Serial.print(F("ac: "); Serial.println(ac);
  Serial.print(F("PIR: "); Serial.println(pir);

  Serial.print(F("target_temperature: "); Serial.println(target_temperature);
  Serial.print(F("operation_mode: "); Serial.println(operation_mode);
  Serial.print(F("away_mode: "); Serial.println(away_mode);
  Serial.print(F("alarm: "); Serial.println(alarm);*/

}

void printLastPage() {
  yoffset -= 1;
  // add/remove 8 pixel for every line yoffset <= -209, if you want to add a line yoffset <= -217
  if (yoffset <= -209) {
    yoffset = SCREEN_HEIGHT + 6;
    lastPageScrollTriggered = true;
  }
  int effectiveOffset = (yoffset >= 0 && !lastPageScrollTriggered) ? 0 : yoffset;

  display.drawBitmap((display.width()-habigLogoW)-1, effectiveOffset, habigLogo, habigLogoW, habigLogoH, 1);
  display.setCursor(0, effectiveOffset);
  display.setTextSize(1);
  #ifdef TARGET_SMARTOSTAT_OLED
    display.println(F("SMARTOSTAT 5.0"));
  #else
    display.println(F("SMARTOLED 5.0"));
  #endif
  display.println(F("by DPsoftware"));
  display.println(F(""));
  
  display.print(F("HA: ")); display.print(F("(")); display.print(haVersion); display.println(F(")"));
  display.print(F("Wifi: ")); display.print(getQuality()); display.println(F("%")); 
  display.print(F("Heap: ")); display.print(ESP.getFreeHeap()/1024); display.println(F(" KB")); 
  display.print(F("Free Flash: ")); display.print(ESP.getFreeSketchSpace()/1024); display.println(F(" KB")); 
  display.print(F("Frequency: ")); display.print(ESP.getCpuFreqMHz()); display.println(F("MHz")); 

  display.print(F("Flash: ")); display.print(ESP.getFlashChipSize()/1024); display.println(F(" KB")); 
  display.print(F("Sketch: ")); display.print(ESP.getSketchSize()/1024); display.println(F(" KB")); 
  display.print(F("IP: ")); display.println(WiFi.localIP());
  display.println(F("MAC: ")); display.println(WiFi.macAddress());
  display.print(F("SDK: ")); display.println(ESP.getSdkVersion());
  display.print(F("Arduino Core: ")); display.println(ESP.getCoreVersion());
  display.println(F("Last Boot: ")); display.println(lastBoot);
  display.println(F("Last WiFi connection:")); display.println(lastWIFiConnection);
  display.println(F("Last MQTT connection:")); display.println(lastMQTTConnection);

  // add/remove 8 pixel for every line effectiveOffset+175, if you want to add a line effectiveOffset+183
  display.drawBitmap((((display.width()/2)-(arduinoLogoW/2))), effectiveOffset+175, arduinoLogo, arduinoLogoW, arduinoLogoH, 1);
}

void drawHeader() {
  display.setTextSize(1);
  display.setCursor(0,0);

  display.print(date);
  display.print(F("      "));
  display.print(currentime);

  display.drawLine(0, 8, display.width()-1, 8, WHITE);

  hours = timedate.substring(11,13);
  minutes = timedate.substring(14,16);
}

void drawFooter() {
  display.setTextSize(1);
  display.setCursor(0,57);
  display.print(target_temperature); (target_temperature != off_cmd) ? display.print(F("C")) : display.print(F(""));
  display.print(F(" "));
  display.print(humidity); display.print(F("%"));
  display.print(F(" "));
  display.print(pressure); display.print(F("hPa"));
}

void drawCenterScreenLogo(bool &triggerBool, const unsigned char* logo, const int logoW, const int logoH, const int delayInt) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.drawBitmap(
    (display.width()  - logoW) / 2,
    (display.height() - logoH) / 2,
    logo, logoW, logoH, 1);
  display.display();
  delay(delayInt);
  triggerBool = false;
}

void drawRoundRect() {
  display.drawRoundRect(47, 19, 72, 27, 10, WHITE);
  display.drawRoundRect(47, 20, 71, 25, 9, WHITE);
  display.drawRoundRect(47, 20, 71, 25, 10, WHITE);
  display.drawRoundRect(47, 20, 71, 25, 8, WHITE);
  display.drawRoundRect(48, 20, 70, 25, 10, WHITE);
}

/********************************** START PROCESS JSON*****************************************/
bool processSmartostatSensorJson(char* message) {

  StaticJsonDocument<BUFFER_SIZE> doc;
  DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Serial.println(F("parseObject() failed 1"));
    return false;
  }

  if (doc.containsKey("BME680")) {
    float temperatureFloat = doc["BME680"]["Temperature"];
    temperature = serialized(String(temperatureFloat,1));
    minTemperature = (temperatureFloat < minTemperature) ? temperatureFloat : minTemperature;
    maxTemperature = (temperatureFloat > maxTemperature) ? temperatureFloat : maxTemperature;
    float humidityFloat = doc["BME680"]["Humidity"];
    humidity = serialized(String(humidityFloat,1));    
    minHumidity = (humidityFloat < minHumidity) ? humidityFloat : minHumidity;
    maxHumidity = (humidityFloat > maxHumidity) ? humidityFloat : maxHumidity;
    float pressureFloat = doc["BME680"]["Pressure"];
    pressure = serialized(String(pressureFloat,1));    
    minPressure = (pressureFloat < minPressure) ? pressureFloat : minPressure;
    maxPressure = (pressureFloat > maxPressure) ? pressureFloat : maxPressure;
    float gasResistanceFloat = doc["BME680"]["GasResistance"];
    gasResistance = serialized(String(gasResistanceFloat,1));    
    minGasResistance = (gasResistanceFloat < minGasResistance) ? gasResistanceFloat : minGasResistance;
    maxGasResistance = (gasResistanceFloat > maxGasResistance) ? gasResistanceFloat : maxGasResistance;
    float IAQFloat = doc["BME680"]["IAQ"]; 
    IAQ = serialized(String(IAQFloat,1));    
    minIAQ = (IAQFloat < minIAQ) ? IAQFloat : minIAQ;
    maxIAQ = (IAQFloat > maxIAQ) ? IAQFloat : maxIAQ;
  }

  return true;
}

bool processSmartostatClimateJson(char* message) {

  StaticJsonDocument<BUFFER_SIZE> doc;
  DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Serial.println(F("parseObject() failed 2"));
    return false;
  }

  if (doc.containsKey("smartostat")) {
    const char* timeConst = doc["Time"];
    // On first boot the timedate variable is OFF
    if (timedate == off_cmd) {
      setDateTime(timeConst);
      lastBoot = date + " " + currentime;
    } else {
      setDateTime(timeConst);
    }
    // lastMQTTConnection and lastWIFiConnection are resetted on every disconnection
    if (lastMQTTConnection == off_cmd) {
      lastMQTTConnection = date + " " + currentime;
    }
    if (lastWIFiConnection == off_cmd) {
      lastWIFiConnection = date + " " + currentime;
    }
    // reset min max
    // if (hours == "23" && minutes == "59") {
    //   resetMinMaxValues();
    // }
    const char* haVersionConst = doc["haVersion"];
    haVersion = haVersionConst;
    const char* operationModeHeatConst = doc["smartostat"]["operation_mode"];
    String operation_mode_heat = operationModeHeatConst;
    const char* operationModeCoolConst = doc["smartostatac"]["operation_mode"];
    String operation_mode_cool = operationModeCoolConst;
    const char* alarmConst = doc["smartostat"]["alarm"];
    alarm = alarmConst;
    if (operation_mode_heat == HEAT) {
      float target_temperatureFloat = doc["smartostat"]["temperature"];
      target_temperature = serialized(String(target_temperatureFloat,1));  ;
      operation_mode = operation_mode_heat;
      const char* awayModeConst = doc["smartostat"]["away_mode"];
      away_mode = awayModeConst;
    } else if (operation_mode_cool == COOL) {
      float target_temperatureFloat = doc["smartostatac"]["temperature"];
      target_temperature = serialized(String(target_temperatureFloat,1));  ;
      operation_mode = operation_mode_cool;
      const char* awayModeConst = doc["smartostatac"]["away_mode"];
      away_mode = awayModeConst;
    } else {
      target_temperature = off_cmd;
      operation_mode = off_cmd;
      away_mode = off_cmd;
    }
  }

  return true;
}

bool processSpotifyStateJson(char* message) {

  StaticJsonDocument<BUFFER_SIZE> doc;
  DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Serial.println(F("parseObject() failed 4"));
    return false;
  }

  unsigned int tempCurrentPage = currentPage;
  if (doc.containsKey("media_artist")) {
    const char* spotifyActivityConst = doc["spotify_activity"];
    spotifyActivity = spotifyActivityConst;
    // if paused clear mediaTitle and other strings and set current page 0
    if (spotifyActivity == "paused" || spotifyActivity == "idle") {
      mediaTitle = "";
      mediaArtist = "";
      spotifySource = "";
      volumeLevel = "";
      currentPage = 0;
    }
    // no one was playing before because mediaTitle is empty, trigger page 4
    if (spotifyActivity == "playing" && mediaTitle == "") {
      currentPage = 7;
    }
    const char* spotifySourceConst = doc["spotifySource"];
    spotifySource = spotifySourceConst;
    const char* volumeLevelConst = doc["volume_level"];
    volumeLevel = volumeLevelConst;
    const char* mediaArtistConst = doc["media_artist"];
    mediaArtist = mediaArtistConst;
    // no mediaTitle if is playing
    if (spotifyActivity != "paused" && spotifyActivity != "idle") {
      const char* mediaTitleConst = doc["media_title"];
      mediaTitle = mediaTitleConst;
    }
    if (mediaTitle == "Bluetooth Audio") {
      currentPage = tempCurrentPage;
    }
  }
  return true;
}

bool processSmartostatPirState(char* message) {
  pir = message;
  return true;
}

bool processSmartoledCmnd(char* message) {
  if(strcmp(message, on_cmd) == 0) {
    stateOn = true;
  } else if(strcmp(message, off_cmd) == 0) {
    stateOn = false;
  }
  sendPowerState();
  return true;
}

bool processSmartostatAcJson(char* message) {
  String msg = message;
  ac = (msg == "off") ? off_cmd : on_cmd;
  if (ac == on_cmd) {
    acTriggered = true;
    //currentPage = 0;
  }
  return true;
}

bool processFurnancedCmnd(char* message) {
  furnance = message;
  if (furnance == on_cmd) {
    furnanceTriggered = true;
  }
  #ifdef TARGET_SMARTOSTAT_OLED
    sendFurnanceState();
    releManagement();
  #endif
  return true;
}

// IRSEND MQTT message ON OFF only for Smartostat
#ifdef TARGET_SMARTOSTAT_OLED
  bool processIrOnOffCmnd(char *message) {
    String acState = message;
    if (acState == on_cmd && ac == off_cmd) {
      acTriggered = true;
      ac = on_cmd;
      acir.on();
      acir.setFan(kSamsungAcFanLow);
      acir.setMode(kSamsungAcCool);
      acir.setTemp(20);
      acir.setSwing(false);
      acir.sendExtended();
      sendACState();
    } else if (acState == off_cmd) {
      ac = off_cmd;
      acir.off();
      acir.sendOff();     
      sendACState(); 
    }
    //Serial.printf("  %s\n", acir.toString().c_str());
    return true;
  }

  bool processIrSendCmnd(char* message) {

    StaticJsonDocument<BUFFER_SIZE> doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
      Serial.println(F("parseObject() failed processIrSendCmnd"));
      return false;
    }

    acir.on();
    if (doc.containsKey("alette_ac")) {      
      acir.setMode(kSamsungAcCool);

      const char* tempConst = doc["temp"];
      uint8_t tempInt = atoi(tempConst);
      
      acir.setTemp(tempInt);

      const char* aletteConst = doc["alette_ac"];
      String alette = aletteConst;
      acir.setQuiet(false);
      acir.setPowerful(false);
      if(alette == "off") {
        acir.setSwing(false);
        const char* modeConst = doc["mode"];
        String mode = modeConst;
        if (mode == "Low") {
          acir.setFan(kSamsungAcFanLow);
        } else if (mode == "Power") {
          acir.setPowerful(true);
        } else if (mode == "Quiet") {        
          acir.setQuiet(true);
        } else if (mode == "Auto") {
          acir.setFan(kSamsungAcFanAuto);
        } else if (mode == "High") {
          acir.setFan(kSamsungAcFanHigh);
        }
      } else {
        acir.setFan(kSamsungAcFanHigh);
        acir.setSwing(true);
      }    
      acir.send();    
      //Serial.printf("  %s\n", acir.toString().c_str()); 
    }
    return true;
  }
#endif

#ifdef TARGET_SMARTOLED
  bool processSmartostatFurnanceState(char* message) {
    furnance = message;
    return true;
  }
  bool processACState(char* message) {
    ac = message;
    return true;
  }  
#endif

void resetMinMaxValues() {
  minTemperature = 99;
  maxTemperature = 0.0;
  minHumidity = 99;
  maxHumidity = 0.0;
  minPressure = 2000;
  maxPressure = 0.0;
  minGasResistance = 2000;
  maxGasResistance = 0.0;
  minIAQ = 2000;
  maxIAQ = 0.0;
}

void setDateTime(const char* timeConst) {
  timedate = timeConst;
  date = timedate.substring(8,10) + "/" + timedate.substring(5,7) + "/" + timedate.substring(0,4);
  currentime = timedate.substring(11,16);
}

/********************************** SEND STATE *****************************************/
void sendPowerState() {
  client.publish(smartoled_state_topic, (stateOn) ? on_cmd : off_cmd, true);
}

void sendInfoState() {
  StaticJsonDocument<BUFFER_SIZE> doc;

  JsonObject root = doc.to<JsonObject>();

  root["State"] = (stateOn) ? on_cmd : off_cmd;
  root["Time"] = timedate;

  char buffer[measureJson(root) + 1];
  serializeJson(root, buffer, sizeof(buffer));

  // publish state only if it has received time from HA
  if (timedate != off_cmd) {
    client.publish(smartoled_info_topic, buffer, true);
  }
}


// Send PIR state via MQTT
#ifdef TARGET_SMARTOSTAT_OLED

  void sendPirState() {   
      client.publish(smartostat_pir_state_topic, (pir == on_cmd) ? on_cmd : off_cmd, true);
  }

  void sendSensorState() {    

    StaticJsonDocument<BUFFER_SIZE> doc;
    JsonObject root = doc.to<JsonObject>();
    
    root["Time"] = timedate;
    root["state"] = (stateOn) ? on_cmd : off_cmd;
    root["POWER1"] = furnance;
    root["POWER2"] = pir;
    JsonObject BME680 = root.createNestedObject("BME680");
    if (readOnceEveryNTimess == 0) {
      if (!boschBME680.performReading()) {
        Serial.println("Failed to perform reading :(");
        delay(500);
        return;
      }
      BME680["Temperature"] = serialized(String(boschBME680.temperature,1));
      BME680["Humidity"] = serialized(String(boschBME680.humidity,1)); 
      BME680["Pressure"] = serialized(String(boschBME680.pressure/100,1)); 
      BME680["GasResistance"] = serialized(String(gas_reference/1000,1)); 
      humidity_score = getHumidityScore();
      gas_score = getGasScore();
      //Combine results for the final IAQ index value (0-100% where 100% is good quality air)
      float air_quality_score = humidity_score + gas_score;
      if ((getgasreference_count++) % 5 == 0) getGasReference();
      BME680["IAQ"] = calculateIAQ(air_quality_score);; 
    } else {
      BME680["Temperature"] = temperature;
      BME680["Humidity"] = humidity;
      BME680["Pressure"] = pressure;
      BME680["GasResistance"] = gasResistance; //gasResistance;
      BME680["IAQ"] = IAQ; //gasResistance;
    }
    readOnceEveryNTimess++;
    // BME680 is in forced mode, it sleeps until it read to avoid self heating
    if (readOnceEveryNTimess == 5) {
      readOnceEveryNTimess = 0;
    }

    char buffer[measureJson(root) + 1];
    serializeJson(root, buffer, sizeof(buffer));

    client.publish(smartostat_sensor_state_topic, buffer, true);
  }

  void sendFurnanceState() {
    if (furnance == off_cmd) {
      forceFurnanceOn = false;
    } 
    client.publish(smartostat_furnance_state_topic, (furnance == off_cmd) ? off_cmd : on_cmd, true);
  }

  void sendACState() {    
    if (ac == off_cmd) {
      forceACOn = false;
    }     
    client.publish(smartostatac_stat_irsend, (ac == off_cmd) ? off_cmd : on_cmd, true);
  }
  
  void sendClimateState(String mode) {
    if (mode == COOL) {
      client.publish(smartostat_cmnd_climate_cool_state, (ac == off_cmd) ? off_cmd : on_cmd, true);
    } else {
      client.publish(smartostat_cmnd_climate_heat_state, (furnance == off_cmd) ? off_cmd : on_cmd, true);
    }      
  }

  void manageSmartostatButton() {
    // Touch button management features
    if (digitalRead(OLED_BUTTON_PIN) == HIGH) {
      lastButtonPressed = OLED_BUTTON_PIN;
      touchButtonManagement(HIGH);
    } else if (digitalRead(SMARTOSTAT_BUTTON_PIN) == HIGH) {
      lastButtonPressed = SMARTOSTAT_BUTTON_PIN;
      touchButtonManagement(HIGH);
    } else {
      touchButtonManagement(LOW);
    }    
    // useful when no Wifi or MQTT answer
    if (lastButtonPressed == SMARTOSTAT_BUTTON_PIN && forceACOn) {
      acManagement();
    }    
    if (lastButtonPressed == SMARTOSTAT_BUTTON_PIN && forceFurnanceOn) {
      releManagement();
    }    
  }

#endif

/********************************** START MQTT RECONNECT*****************************************/
void mqttReconnect() {
  // how many attemps to MQTT connection
  int brokermqttcounter = 0;
  // Loop until we're reconnected
  while (!client.connected()) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    if (brokermqttcounter <= 20) {
      display.println(F("Connecting to"));
      display.println(F("MQTT Broker..."));
    } else {
      display.println(F("Keep pressed for half\na second to toggle\nfurnance."));
    }
    // display.println(F("MQTT Broker...");
    display.display();

    // used to manage furnance state when MQTT is disconnected
    #ifdef TARGET_SMARTOSTAT_OLED
      manageSmartostatButton();
    #endif

    // Attempt to connect
    if (client.connect(SENSORNAME, mqtt_username, mqtt_password)) {
      Serial.println(F("connected"));
      display.println(F(""));
      display.println(F("CONNECTED"));
      display.println(F(""));
      display.println(F("Reading data from"));
      display.println(F("the network..."));
      display.display();
      client.subscribe(smartostat_climate_state_topic);
      client.subscribe(smartostat_sensor_state_topic);
      client.subscribe(smartostat_state_topic);
      #ifdef TARGET_SMARTOLED
        client.subscribe(smartostat_furnance_state_topic);     
        client.subscribe(smartostat_pir_state_topic);
        client.subscribe(smartostatac_cmd_topic);
        client.subscribe(smartostatac_stat_irsend);        
      #endif
      client.subscribe(spotify_state_topic);
      client.subscribe(smartoled_cmnd_topic);
      client.subscribe(smartostat_furnance_cmnd_topic);     
      #ifdef TARGET_SMARTOSTAT_OLED       
        client.subscribe(smartostatac_cmnd_irsendState);    
        client.subscribe(smartostatac_cmnd_irsend);           
      #endif
      delay(delay_2000);
      brokermqttcounter = 0;
      // reset the lastMQTTConnection to off, will be initialized by next time update
      lastMQTTConnection = off_cmd;
    } else {
      display.println(F("Number of attempts="));
      display.println(brokermqttcounter);
      display.display();
      // after 10 attemps all peripherals are shut down
      if (brokermqttcounter >= MAX_RECONNECT) {
        display.println(F("Max retry reached, powering off peripherals."));
        display.display();
        // shut down if wifi disconnects
        #ifdef TARGET_SMARTOSTAT_OLED
          furnance = forceFurnanceOn ? on_cmd : off_cmd;
          releManagement();
          ac = forceACOn ? on_cmd : off_cmd;
          acManagement();
        #endif
      } else if (brokermqttcounter > 10000) {
        brokermqttcounter = 0;
      }
      brokermqttcounter++;
      // Wait 5 seconds before retrying
      delay(500);
    }
  }
}

// Send status to MQTT broker every ten seconds
void delayAndSendStatus() {
  if(millis() > timeNowStatus + tenSecondsPeriod){
    timeNowStatus = millis();
    ledTriggered = true;
    sendPowerState();
    sendInfoState();
    #ifdef TARGET_SMARTOSTAT_OLED
      sendSensorState();
      sendFurnanceState();
      sendACState();
    #endif
  }
}

// Go to home page after five minutes of inactivity and write SPIFFS
void goToHomePageAndWriteSPIFFSAfterFiveMinutes() {
  if(millis() > timeNowGoHomeAfterFiveMinutes + fiveMinutesPeriod){
    timeNowGoHomeAfterFiveMinutes = millis();
    // Write data to file system
    writeConfigToSPIFFS();
    if ((humidity != "OFF" && humidity.toFloat() < humidityThreshold) && ((spotifyActivity == "playing" && currentPage != 5) || spotifyActivity != "playing")) {
      currentPage = 0;
    }
  }
}

/********************************** PIR AND RELE' MANAGEMENT *****************************************/
#ifdef TARGET_SMARTOSTAT_OLED

  void pirManagement() {
    if (digitalRead(SR501_PIR_PIN) == HIGH) {
      if (pir == off_cmd) {
        highIn = millis();
        pir = on_cmd;
      }
      if (pir == on_cmd) {
        if ((millis() - highIn) > 500 ) { // 7000 four seconds on time
          if (lastPirState != on_cmd) {
            lastPirState = on_cmd;
            sendPirState();
          }
          highIn = millis();
        }
      }
    }
    if (digitalRead(SR501_PIR_PIN) == LOW) { 
      highIn = millis();     
      if (pir == on_cmd) {
        pir = off_cmd;
        if (lastPirState != off_cmd) {
          lastPirState = off_cmd;
          sendPirState();
        }
      }      
    }
  }

  void releManagement() {
    if (furnance == on_cmd || forceFurnanceOn) {
      digitalWrite(RELE_PIN, HIGH);
    } else {
      digitalWrite(RELE_PIN, LOW);
    }
  }

  void acManagement() {
    if (ac == on_cmd || forceACOn) {
      acir.on();
      acir.setFan(kSamsungAcFanLow);
      acir.setMode(kSamsungAcCool);
      acir.setTemp(20);
      acir.setSwing(false);
      acir.sendExtended();
    } else {
      acir.off();
      acir.sendOff();     
    }
  }

  void getGasReference() {
    // Now run the sensor for a burn-in period, then use combination of relative humidity and gas resistance to estimate indoor air quality as a percentage.
    //Serial.println("Getting a new gas reference value");
    int readings = 10;
    for (int i = 1; i <= readings; i++) { // read gas for 10 x 0.150mS = 1.5secs
      gas_reference += boschBME680.readGas();
    }
    gas_reference = gas_reference / readings;
    //Serial.println("Gas Reference = "+String(gas_reference,3));
    getgasreference_count = 0;
  }

  String calculateIAQ(int score) {
    score = (100 - score) * 5;
    // if      (score >= 301)                  IAQ_text += "Hazardous";
    // else if (score >= 201 && score <= 300 ) IAQ_text += "Very Unhealthy";
    // else if (score >= 176 && score <= 200 ) IAQ_text += "Unhealthy";
    // else if (score >= 151 && score <= 175 ) IAQ_text += "Unhealthy for Sensitive Groups";
    // else if (score >=  51 && score <= 150 ) IAQ_text += "Moderate";
    // else if (score >=  00 && score <=  50 ) IAQ_text += "Good";
    // Serial.print("IAQ Score = " + String(score) + ", ");
    return String(score);
  }

  int getHumidityScore() {  //Calculate humidity contribution to IAQ index
    float current_humidity = boschBME680.humidity;
    if (current_humidity >= 38 && current_humidity <= 42) // Humidity +/-5% around optimum
      humidity_score = 0.25 * 100;
    else
    { // Humidity is sub-optimal
      if (current_humidity < 38)
        humidity_score = 0.25 / hum_reference * current_humidity * 100;
      else
      {
        humidity_score = ((-0.25 / (100 - hum_reference) * current_humidity) + 0.416666) * 100;
      }
    }
    return humidity_score;
  }

  int getGasScore() {
    //Calculate gas contribution to IAQ index
    gas_score = (0.75 / (gas_upper_limit - gas_lower_limit) * gas_reference - (gas_lower_limit * (0.75 / (gas_upper_limit - gas_lower_limit)))) * 100.00;
    if (gas_score > 75) gas_score = 75; // Sometimes gas readings can go outside of expected scale maximum
    if (gas_score <  0) gas_score = 0;  // Sometimes gas readings can go outside of expected scale minimum
    return gas_score;
  }

#endif

/********************************** TOUCH BUTTON MANAGEMENT *****************************************/
void touchButtonManagement(int digitalReadButtonPin) {
  buttonState = digitalReadButtonPin;
  // Quick presses
  if (buttonState == HIGH && lastReading == LOW) { // function triggered on the quick press of the button
    onTime = millis();
    pressed = true;
    quickPress();
  } else if (buttonState == LOW && pressed) { // function triggered on the release of the button
    pressed = false;
    if (longPress) { // button release long pressed
      longPress = false;
      longPressRelease();
    } else { // button release no long press
      quickPressRelease();
    }
  }
  // Long press for a second
  if (buttonState == HIGH && lastReading == HIGH) {
    if ((millis() - onTime) > 1000 ) { // a second hold time
      lastReading = LOW;
      longPress = true;
    }
  }
  lastReading = buttonState;
}

void longPressRelease() {
  // turn off the display on long pressed
  stateOn = false;
  resetMinMaxValues();
  writeConfigToSPIFFS();
}

void quickPress() {
  // reset the go to homepage timer on quick button press
  timeNowGoHomeAfterFiveMinutes = millis();
}

void quickPressRelease() {
  // turn on the furnance
  if (lastButtonPressed == SMARTOSTAT_BUTTON_PIN) {
    #ifdef TARGET_SMARTOSTAT_OLED
      if (temperature.toFloat() > 25) {
        if (ac == on_cmd) {
          ac = off_cmd;
          // stop ac on long press if wifi or mqtt is disconnected
          forceACOn = false;
        } else {
          acTriggered = true;
          ac = on_cmd;
          // start ac on long press if wifi or mqtt is disconnected
          forceACOn = true;
        }      
        sendACState();
        sendClimateState(COOL);
        if (lastButtonPressed == SMARTOSTAT_BUTTON_PIN) {
          acManagement();
        }   
        lastButtonPressed = OLED_BUTTON_PIN;
      } else {
        if (furnance == on_cmd) {
          furnance = off_cmd;
          // stop furnance on long press if wifi or mqtt is disconnected
          forceFurnanceOn = false;
        } else {
          furnanceTriggered = true;
          furnance = on_cmd;
          // start furnance on long press if wifi or mqtt is disconnected
          forceFurnanceOn = true;
        }      
        sendFurnanceState();
        sendClimateState(HEAT);
        if (lastButtonPressed == SMARTOSTAT_BUTTON_PIN) {
          releManagement();
        }   
        lastButtonPressed = OLED_BUTTON_PIN;
      }      
    #endif
  } else {
    // go to the next page if display is on, skip next page if the display was off
    if (stateOn) {
      currentPage++;
      // reset flag that makes the last page scroll correctly the first time is triggered after a page change
      lastPageScrollTriggered = false;
      yoffset = 150;
    }
    // button pressed, if display is off turn it on
    if (stateOn == false) {
      stateOn = true;
      sendPowerState();
    }
    // Go back to first page if it's the last page
    if (currentPage > numPages) {
      currentPage = 0;
    }
  }
}


/********************************** SPIFFS MANAGEMENT *****************************************/
void readConfigFromSPIFFS() {
  bool error = false;
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println(F("Mounting SPIFSS..."));
  display.display();
  // SPIFFS.remove("/config.json");
  if (SPIFFS.begin()) {
    display.println(F("FS mounted"));
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      display.println(F("Reading config.json file..."));
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        display.println(F("Config OK"));
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument doc(1024);        
        DeserializationError deserializeError = deserializeJson(doc, buf.get());
        Serial.println(F("\nReading config.json"));
        serializeJsonPretty(doc, Serial);
        if (!deserializeError) {
          display.println(F("JSON parsed"));
          if(doc["minTemperature"]) {
            display.println(F("Reload previously stored values."));
            minTemperature = doc["minTemperature"];
            maxTemperature = doc["maxTemperature"];
            minHumidity = doc["minHumidity"];
            maxHumidity = doc["maxHumidity"];
            minPressure = doc["minPressure"];
            maxPressure = doc["maxPressure"];
            minGasResistance = doc["minGasResistance"];
            maxGasResistance = doc["maxGasResistance"];
            minIAQ = doc["minIAQ"];
            maxIAQ = doc["maxIAQ"];
          } else {
            error = true;
            display.println(F("JSON file is empty"));
          }
        } else {
          error = true;
          display.println(F("Failed to load json config"));
        }
      }
    }
  } else {
    error = true;
    display.println(F("failed to mount FS"));
  }
  display.display();
  if (!error) {
    delay(delay_4000);
  }
}

void writeConfigToSPIFFS() {
    if (SPIFFS.begin()) {
      Serial.println(F("\nSaving config.json\n"));
      DynamicJsonDocument doc(1024);
      doc["minTemperature"] = minTemperature;
      doc["maxTemperature"] = maxTemperature;
      doc["minHumidity"] = minHumidity;
      doc["maxHumidity"] = maxHumidity;
      doc["minPressure"] = minPressure;
      doc["maxPressure"] = maxPressure;
      doc["minGasResistance"] = minGasResistance;
      doc["maxGasResistance"] = maxGasResistance;
      doc["minIAQ"] = minIAQ;
      doc["maxIAQ"] = maxIAQ;
      // SPIFFS.format();
      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println(F("Failed to open config file for writing"));
      }
      serializeJsonPretty(doc, Serial);
      serializeJson(doc, configFile);
      configFile.close();
    } else {
      Serial.println(F("Failed to mount FS for write"));
    }
}

// https://stackoverflow.com/questions/9072320/split-string-into-string-array
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}


/********************************** START SETUP WIFI *****************************************/
void setup_wifi() {

  unsigned int reconnectAttemp = 0;

  // DPsoftware domotics
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(5,17);
  display.drawRoundRect(0, 0, display.width()-1, display.height()-1, display.height()/4, WHITE);
  display.println(F("DPsoftware domotics"));
  display.display();

  delay(delay_3000);

  // Read config.json from SPIFFS
  readConfigFromSPIFFS();

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println(F("Connecting to: "));
  display.print(ssid); display.println(F("..."));
  display.println(F("\nKeep pressed for half\na second to toggle\nfurnance."));
  display.display();

  Serial.println();
  Serial.print(F("Connecting to "));
  Serial.print(ssid);  

  delay(delay_2000);

  WiFi.persistent(false);   // Solve possible wifi init errors (re-add at 6.2.1.16 #4044, #4083)
  WiFi.disconnect(true);    // Delete SDK wifi config
  delay(200);
  WiFi.mode(WIFI_STA);      // Disable AP mode
  //WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoConnect(true);
  // IP of the arduino, dns, gateway
  #ifdef TARGET_SMARTOSTAT_OLED
    WiFi.config(arduinoip_smartostat, mydns, mygateway);
  #endif 
  #ifdef TARGET_SMARTOLED
    WiFi.config(arduinoip, mydns, mygateway);
  #endif  

  WiFi.hostname(SENSORNAME);

  // Set wifi power in dbm range 0/0.25, set to 0 to reduce PIR false positive due to wifi power
  WiFi.setOutputPower(0);

  WiFi.begin(ssid, password);

  // loop here until connection
  while (WiFi.status() != WL_CONNECTED) {
    #ifdef TARGET_SMARTOSTAT_OLED
      manageSmartostatButton();  
      if (reconnectAttemp >= MAX_RECONNECT) {
        // Start with furnance off, shut down if wifi disconnects
        furnance = forceFurnanceOn ? on_cmd : off_cmd;
        releManagement();
        ac = forceACOn ? on_cmd : off_cmd;
        acManagement();    
      }       
    #endif
    delay(500);
    Serial.print(F("."));
    reconnectAttemp++;
    if (reconnectAttemp > 10) {
      display.setCursor(0,0);
      display.clearDisplay();
      display.print(F("Reconnect attemp= "));
      display.print(reconnectAttemp);
      if (reconnectAttemp >= MAX_RECONNECT) {
        display.println(F("Max retry reached, powering off peripherals."));
      }
      display.display();
    } else if (reconnectAttemp > 10000) {
      reconnectAttemp = 0;
    }
  }

  display.println(F("WIFI CONNECTED"));
  display.println(WiFi.localIP());
  display.display();

  // reset the lastWIFiConnection to off, will be initialized by next time update
  lastWIFiConnection = off_cmd;

  delay(delay_1500);

}

/*
   Return the quality (Received Signal Strength Indicator) of the WiFi network.
   Returns a number between 0 and 100 if WiFi is connected.
   Returns -1 if WiFi is disconnected.
*/
int getQuality() {
  if (WiFi.status() != WL_CONNECTED)
    return -1;
  int dBm = WiFi.RSSI();
  if (dBm <= -100)
    return 0;
  if (dBm >= -50)
    return 100;
  return 2 * (dBm + 100);
}

// Blink LED_BUILTIN without bloking delay
void nonBlokingBlink() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval && ledTriggered) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    // blink led
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    blinkCounter++;
    if (blinkCounter >= blinkTimes) {
      blinkCounter = 0;
      ledTriggered = false;
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }  
}

/********************************** START SETUP*****************************************/
void setup() {
  
  #ifdef TARGET_SMARTOSTAT_OLED
    // IRSender Begin
    acir.begin();
    Serial.begin(115200);

    // SR501 PIR sensor
    pinMode(SR501_PIR_PIN, INPUT);
    digitalWrite(SR501_PIR_PIN, LOW);

    // Touch button used to start/stop the furnance
    pinMode(SMARTOSTAT_BUTTON_PIN, INPUT);

    // Relè PIN
    pinMode(RELE_PIN, OUTPUT);

    // BME680 initialization
    if (!boschBME680.begin(0x76)) {
      Serial.println("Could not find a valid BME680 sensor, check wiring!");
      while (1);
    }
    boschBME680.setTemperatureOversampling(BME680_OS_1X);
    boschBME680.setHumidityOversampling(BME680_OS_1X);
    boschBME680.setPressureOversampling(BME680_OS_1X);
    boschBME680.setIIRFilterSize(BME680_FILTER_SIZE_0);
    boschBME680.setGasHeater(320, 150); // 320*C for 150 ms   
    // Now run the sensor to normalise the readings, then use combination of relative humidity and gas resistance to estimate indoor air quality as a percentage.
    // The sensor takes ~30-mins to fully stabilise
    getGasReference(); 
    delay(30);

    acir.off();
    acir.setFan(kSamsungAcFanLow);
    acir.setMode(kSamsungAcCool);
    acir.setTemp(25);
    acir.setSwing(false);
    //Serial.printf("  %s\n", acir.toString().c_str());

    // Display Rotation 180°
    display.setRotation(2);

  #else 
    Serial.begin(serialRate);
  #endif

  // OLED TouchButton
  pinMode(OLED_BUTTON_PIN, INPUT);
  
  // LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display.setTextColor(WHITE);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  //OTA SETUP
  ArduinoOTA.setPort(OTAport);
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(SENSORNAME);

  // No authentication by default
  ArduinoOTA.setPassword((const char *)OTApassword);

  ArduinoOTA.onStart([]() {
    Serial.println(F("Starting"));
  });
  ArduinoOTA.onEnd([]() {
    Serial.println(F("\nEnd"));
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
  });
  ArduinoOTA.begin();

  Serial.println(F("Ready"));
  Serial.print(F("IP Address: "));
  Serial.println(WiFi.localIP());

}

/********************************** START MAIN LOOP *****************************************/
void loop() {  
  
  // Wifi management
  if (WiFi.status() != WL_CONNECTED) {
    delay(1);
    Serial.print(F("WIFI Disconnected. Attempting reconnection."));
    setup_wifi();
    return;
  }

  ArduinoOTA.handle();

  if (!client.connected()) {
    mqttReconnect();
  }
  client.loop();

  // PIR and RELE' MANAGEMENT 
  #ifdef TARGET_SMARTOSTAT_OLED    
    manageSmartostatButton();    
    pirManagement();
  #else
    lastButtonPressed = OLED_BUTTON_PIN;
    touchButtonManagement(digitalRead(OLED_BUTTON_PIN));
  #endif

  // // Draw Speed, it influences how long the button should be pressed before switching to the next currentPage
  delay(20);

  // // Send status on MQTT Broker every n seconds
  delayAndSendStatus();

  // DRAW THE SCREEN
  if (stateOn) {
    draw();
  } else {
    display.clearDisplay();
    display.display();
  }

  // Go To Home Page timer after 5 minutes of inactivity and write data to File System (SPIFFS)
  goToHomePageAndWriteSPIFFSAfterFiveMinutes();

  nonBlokingBlink();

}