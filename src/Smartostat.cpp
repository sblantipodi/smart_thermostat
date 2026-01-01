/*
  Smartostat.h - Smart Thermostat based on Arduino SDK
  
  Copyright © 2020 - 2026  Davide Perini
  
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

#include <FS.h> //this needs to be first, or it all crashes and burns...
#include <Smartostat.h>


/********************************** START SETUP *****************************************/
void setup() {
#if defined(ARDUINO_ARCH_ESP32)
  rgbLedWrite(LED_BUILTIN, 0, 0, 255);
#endif

#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
  // IRSender Begin
  pinMode(kIrLed, OUTPUT);
  acir.begin();
  acir.calibrate();
  Serial.begin(SERIAL_RATE);
  Serial.setTimeout(0);
#if defined(ARDUINO_ARCH_ESP32)
  Serial.setTxTimeoutMs(0);
#endif

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
#if defined(ESP8266)
    ESP.wdtFeed();
#endif
    delay(50);
    sensorOk = false;
  } else {
    sensorOk = true;
    boschBME680.setTemperatureOversampling(BME680_OS_8X); // BME680_OS_1X/BME680_OS_8X
    boschBME680.setHumidityOversampling(BME680_OS_2X); // BME680_OS_1X/BME680_OS_2X
    boschBME680.setPressureOversampling(BME680_OS_4X); // BME680_OS_1X/BME680_OS_4X
    boschBME680.setIIRFilterSize(BME680_FILTER_SIZE_0); // BME680_FILTER_SIZE_0/BME680_FILTER_SIZE_3
    boschBME680.setGasHeater(320, 150); // 320*C for 150 ms
    // Now run the sensor to normalise the readings, then use combination of relative humidity and gas resistance to estimate indoor air quality as a percentage.
    // The sensor takes ~30-mins to fully stabilise
    getGasReference();
    delay(30);
  }

  acir.stateReset();
  acir.off();
  acir.setFan(kSamsungAcFanLow);
  acir.setMode(kSamsungAcCool);
  acir.setTemp(26);
  acir.setSwing(false);
  //Serial.printf("  %s\n", acir.toString().c_str());

  // Display Rotation 180°
  display.setRotation(2);

#else
  Serial.begin(SERIAL_RATE);
  Serial.setTimeout(0);
#if defined(ARDUINO_ARCH_ESP32)
  Serial.setTxTimeoutMs(0);
#endif
#endif

  // Wait for the serial connection to be establised.
#if defined(ESP8266)
  while (!Serial) {
#if defined(ESP8266)
  ESP.wdtFeed();
#endif
  delay(50);
  }
#endif

  // OLED TouchButton
  pinMode(OLED_BUTTON_PIN, INPUT);

  // LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    // Address 0x3D for 128x64
#else
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {  // Address 0x3D for 128x64
#endif
    Serial.println(F("SSD1306 allocation failed"));
#if defined(ESP8266)
    ESP.wdtFeed();
#endif
    delay(50);
    for (;;); // Don't proceed, loop forever
  }

  display.setTextColor(WHITE);
#if defined(ARDUINO_ARCH_ESP32)
  rgbLedWrite(LED_BUILTIN, 0, 0, 255);
#endif
  // Bootsrap setup() with Wifi and MQTT functions
  bootstrapManager.bootstrapSetup(manageDisconnections, manageHardwareButton, callback);

  readConfigFromStorage();
#if defined(ARDUINO_ARCH_ESP32)
  rgbLedWrite(LED_BUILTIN, 0, 0, 0);
#endif
}

/********************************** MANAGE WIFI AND MQTT DISCONNECTION *****************************************/
void manageDisconnections() {
  // shut down if wifi disconnects
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
  furnance = forceFurnanceOn ? ON_CMD : OFF_CMD;
  releManagement();
  ac = forceACOn ? ON_CMD : OFF_CMD;
  acManagement();
#endif
}

/********************************** MQTT SUBSCRIPTIONS *****************************************/
void manageQueueSubscription() {
  bootstrapManager.subscribe(SMARTOSTAT_CLIMATE_STATE_TOPIC);
  bootstrapManager.subscribe(SMARTOSTAT_SENSOR_STATE_TOPIC);
  bootstrapManager.subscribe(SMARTOSTAT_STATE_TOPIC);
  bootstrapManager.subscribe(UPS_STATE);
#if defined(TARGET_SMARTOLED) || defined(TARGET_SMARTOLED_ESP32)
  bootstrapManager.subscribe(SMARTOSTAT_FURNANCE_STATE_TOPIC);
  bootstrapManager.subscribe(SMARTOSTAT_PIR_STATE_TOPIC);
  bootstrapManager.subscribe(SMARTOSTATAC_CMD_TOPIC);
  bootstrapManager.subscribe(SMARTOSTATAC_STAT_IRSEND);
  bootstrapManager.subscribe(SMARTOLED_CMND_REBOOT);
  bootstrapManager.subscribe(LUCIFERIN_FRAMERATE);
  bootstrapManager.subscribe(GLOWORM_FRAMERATE);
#endif
  bootstrapManager.subscribe(SPOTIFY_STATE_TOPIC);
  bootstrapManager.subscribe(SMARTOLED_CMND_TOPIC);
  bootstrapManager.subscribe(SMARTOSTAT_FURNANCE_CMND_TOPIC);
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
  bootstrapManager.subscribe(SMARTOSTAT_CMND_REBOOT);
  bootstrapManager.subscribe(SMARTOSTATAC_CMND_IRSENDSTATE);
  bootstrapManager.subscribe(SMARTOSTATAC_CMND_IRSEND);
  bootstrapManager.subscribe(TOGGLE_BEEP);
#endif
  bootstrapManager.subscribe(SOLAR_STATION_POWER_STATE);
  bootstrapManager.subscribe(SOLAR_STATION_PUMP_POWER);
  bootstrapManager.subscribe(SOLAR_STATION_STATE);
  bootstrapManager.subscribe(SOLAR_STATION_REMAINING_SECONDS);
  bootstrapManager.subscribe(CMND_IR_RECEV);
}

/********************************** MANAGE HARDWARE BUTTON *****************************************/
void manageHardwareButton() {
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
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
#endif
}

/********************************** START CALLBACK *****************************************/
void callback(char *topic, byte *payload, unsigned int length) {
  JsonDocument json = bootstrapManager.parseQueueMsg(topic, payload, length);

  if (strcmp(topic, SMARTOSTAT_SENSOR_STATE_TOPIC) == 0) {
    processSmartostatSensorJson(json);
  } else if (strcmp(topic, SMARTOSTAT_CLIMATE_STATE_TOPIC) == 0) {
    processSmartostatClimateJson(json);
  } else if (strcmp(topic, SMARTOSTAT_STATE_TOPIC) == 0) {
    processSmartostatSensorJson(json);
  } else if (strcmp(topic, UPS_STATE) == 0) {
    processUpsStateJson(json);
  } else if (strcmp(topic, SPOTIFY_STATE_TOPIC) == 0) {
    processSpotifyStateJson(json);
  } else if (strcmp(topic, SMARTOSTAT_PIR_STATE_TOPIC) == 0) {
    processSmartostatPirState(json);
  } else if (strcmp(topic, SMARTOLED_CMND_TOPIC) == 0) {
    processSmartoledCmnd(json);
  } else if (strcmp(topic, SMARTOSTAT_FURNANCE_CMND_TOPIC) == 0) {
    processFurnancedCmnd(json);
  } else if (strcmp(topic, SOLAR_STATION_POWER_STATE) == 0) {
    processSolarStationPowerState(json);
  } else if (strcmp(topic, SOLAR_STATION_PUMP_POWER) == 0) {
    processSolarStationWaterPump(json);
  } else if (strcmp(topic, SOLAR_STATION_STATE) == 0) {
    processSolarStationState(json);
  } else if (strcmp(topic, SOLAR_STATION_REMAINING_SECONDS) == 0) {
    processSolarStationRemainingSeconds(json);
  } else if (strcmp(topic, CMND_IR_RECEV) == 0) {
    processIrRecev(json);
  }

#if defined(TARGET_SMARTOLED) || defined(TARGET_SMARTOLED_ESP32)
  if (strcmp(topic, SMARTOSTATAC_CMD_TOPIC) == 0) {
    processSmartostatAcJson(json);
  } else if (strcmp(topic, SMARTOSTAT_FURNANCE_STATE_TOPIC) == 0) {
    processSmartostatFurnanceState(json);
  } else if (strcmp(topic, SMARTOSTATAC_STAT_IRSEND) == 0) {
    processACState(json);
  } else if (strcmp(topic, SMARTOLED_CMND_REBOOT) == 0) {
    processSmartoledRebootCmnd(json);
  } else if (strcmp(topic, LUCIFERIN_FRAMERATE) == 0) {
    processSmartoledFramerate(json);
  } else if (strcmp(topic, GLOWORM_FRAMERATE) == 0) {
    processSmartoledGlowWormFramerate(json);
  }
#endif

#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
  if (strcmp(topic, SMARTOSTATAC_CMND_IRSENDSTATE) == 0) {
    processIrOnOffCmnd(json);
  } else if (strcmp(topic, SMARTOSTATAC_CMND_IRSEND) == 0) {
    processIrSendCmnd(json);
  } else if (strcmp(topic, SMARTOSTAT_CMND_REBOOT) == 0) {
    processSmartostatRebootCmnd(json);
  } else if (strcmp(topic, TOGGLE_BEEP) == 0) {
    toggleBeep(json);
  }
#endif
}

void draw() {
  // pagina 0,1,2,3,4,5,6 sono fisse e sono temp, humidita, pressione, min-maximum, ups, spotify
  // lastPage contiene le info su smartoled
  display.clearDisplay();
  // currentPage = 7;

  if (currentPage == 8) {
    if (spotifyActivity != SPOTIFY_PLAYING) {
      display.clearDisplay();
      currentPage = numPages;
      bootstrapManager.drawInfoPage(VERSION, AUTHOR);
      return;
    }
  }

  if (currentPage != numPages && currentPage != 8) {
    drawHeader();
  }

  // Draw Images
  if (alarmo == ALARM_ARMED_AWAY || alarmo == ALARM_PENDING || alarmo == ALARM_TRIGGERED) {
    display.drawBitmap(0, 10, shieldLogo, shieldLogoW, shieldLogoH, 1);
  } else if (away_mode == OFF_CMD && currentPage != numPages) {
    display.drawBitmap(0, 10, haSmallLogo, haSmallLogoW, haSmallLogoH, 1);
  }

  if (humidity != OFF_CMD && humidity.toFloat() >= humidityThreshold) {
    currentPage = 0;
    display.drawBitmap(14, 18, humidityLogo, humidityLogoW, humidityLogoH, 1);
  } else if (furnance == ON_CMD && currentPage == 0) {
    drawRoundRect();
    display.drawBitmap(14, 18, fireLogo, fireLogoW, fireLogoH, 1);
  } else if (ac == ON_CMD && currentPage == 0) {
    drawRoundRect();
    display.drawBitmap(16, 19, snowLogo, snowLogoW, snowLogoH, 1);
    display.setCursor(3, 30);
    if (fan == FAN_LOW) {
      display.print(F("L"));
    } else if (fan == FAN_HIGH) {
      display.print(F("H"));
    } else if (fan == FAN_AUTO) {
      display.print(F("A"));
    } else if (fan == FAN_POWER) {
      display.print(F("P"));
    } else if (fan == FAN_QUIET) {
      display.print(F("Q"));
    } else if (fan == FAN_WARM) {
      display.print(F("W"));
    }
    display.drawCircle(5, 33, 5, WHITE);
  } else if (hvac_action == HEAT && currentPage == 0) {
    display.drawBitmap(9, 18, tempLogo, tempLogoW, tempLogoH, 1);
  } else if (hvac_action == COOL && currentPage == 0) {
    display.drawBitmap(9, 18, coolLogo, coolLogoW, coolLogoH, 1);
  } else if (currentPage == 1) {
    display.drawBitmap(15, 18, humidityBigLogo, humidityBigLogoW, humidityBigLogoH, 1);
  } else if (currentPage == 2) {
    display.drawBitmap(5, 18, tachimeterLogo, tachimeterLogoW, tachimeterLogoH, 1);
  } else if (currentPage == 3) {
    display.drawBitmap(5, 18, omegaLogo, omegaLogoW, omegaLogoH, 1);
  } else if (currentPage == 4) {
    if (IAQ.toInt() >= 301) display.drawBitmap(5, 18, biohazardLogo, biohazardLogoW, biohazardLogoH, 1);
    else if (IAQ.toInt() >= 201 && IAQ.toInt() <= 300) display.drawBitmap(5, 18, skullLogo, skullLogoW, skullLogoH, 1);
    else if (IAQ.toInt() >= 151 && IAQ.toInt() <= 200) display.drawBitmap(5, 18, smogLogo, smogLogoW, smogLogoH, 1);
    else if (IAQ.toInt() >= 51 && IAQ.toInt() <= 150) display.drawBitmap(5, 18, leafLogo, leafLogoW, leafLogoH, 1);
    else if (IAQ.toInt() >= 00 && IAQ.toInt() <= 50)
      display.drawBitmap(5, 18, butterflyLogo, butterflyLogoW,
                         butterflyLogoH, 1);
  } else if (currentPage == 5) {
    if (hvac_action == HEAT) {
      display.drawBitmap(((display.width() / 3) / 2) - (tempLogoW / 2), 15, tempLogo, tempLogoW, tempLogoH, 1);
    } else if (hvac_action == COOL) {
      display.drawBitmap(((display.width() / 3) / 2) - (coolLogoW / 2), 15, coolLogo, coolLogoW, coolLogoH, 1);
    } else {
      display.drawBitmap(((display.width() / 3) / 2) - (offLogoW / 2), 15, offLogo, offLogoW, offLogoH, 1);
    }
    display.drawBitmap((display.width() / 2) - (humidityBigLogoW / 2), 15, humidityBigLogo, humidityBigLogoW,
                       humidityBigLogoH, 1);
    display.drawBitmap(display.width() - ((display.width() / 3) / 2) - (tachimeterLogoW / 2), 15, tachimeterLogo,
                       tachimeterLogoW, tachimeterLogoH, 1);
  } else if (currentPage == 6) {
    int dispDivide = display.width() / 5;
    display.drawBitmap((dispDivide), 15, omegaLogo, omegaLogoW, omegaLogoH, 1);
    if (IAQ.toInt() >= 301) display.drawBitmap((dispDivide * 3), 15, biohazardLogo, biohazardLogoW, biohazardLogoH, 1);
    else if (IAQ.toInt() >= 201 && IAQ.toInt() <= 300)
      display.drawBitmap(
        (dispDivide * 3), 15, skullLogo, skullLogoW, skullLogoH, 1);
    else if (IAQ.toInt() >= 151 && IAQ.toInt() <= 200)
      display.drawBitmap(
        (dispDivide * 3), 15, smogLogo, smogLogoW, smogLogoH, 1);
    else if (IAQ.toInt() >= 51 && IAQ.toInt() <= 150)
      display.drawBitmap(
        (dispDivide * 3), 15, leafLogo, leafLogoW, leafLogoH, 1);
    else if (IAQ.toInt() >= 00 && IAQ.toInt() <= 50)
      display.drawBitmap((dispDivide * 3), 15, butterflyLogo,
                         butterflyLogoW, butterflyLogoH, 1);
  } else if (currentPage == 7) {
    display.drawBitmap(8, 15, upsLogo, upsLogoW, upsLogoH, 1);
    drawUpsFooter();
  } else if (currentPage == 8) {
    // do nothing here image is shown in the text area
  } else if (currentPage == numPages) {
    // display.drawBitmap((display.width()-habigLogoW)-1, 0, habigLogo, habigLogoW, habigLogoH, 1);
  } else {
    currentPage = 0;
    display.drawBitmap(10, 18, offLogo, offLogoW, offLogoH, 1);
  }

  if (currentPage != 5 && currentPage != 6 && currentPage != 7 && currentPage != 8 && currentPage != numPages) {
    manageFooter();
  }


  // Draw Text
  display.setTextSize(2);

  if (humidity != OFF_CMD && humidity.toFloat() >= humidityThreshold) {
    currentPage = 0;
    display.setCursor(55, 14);
    display.print(humidity);
    display.println(F("%"));
    display.setCursor(55, 35);
    display.print(temperature);
    display.print(F("C"));
  } else if (currentPage == 0) {
    display.setCursor(55, 25);
    display.print(temperature);
    display.print(F("C"));
  } else if (currentPage == 1) {
    display.setCursor(55, 25);
    display.print(humidity);
    display.print(F("%"));
  } else if (currentPage == 2) {
    display.setCursor(35, 25);
    display.print(pressure);
    display.setTextSize(1);
    display.print(F("hPa"));
  } else if (currentPage == 3) {
    display.setCursor(35, 25);
    display.print(gasResistance);
    display.setTextSize(1);
    display.print(F("KOhms"));
  } else if (currentPage == 4) {
    display.setCursor(40, 25);
    display.print(IAQ);
    display.setTextSize(1);
    display.print(F("IAQ"));
  } else if (currentPage == 5) {
    display.setTextSize(1);

    display.setCursor(8, 47);
    display.print(minTemperature, 1);
    display.println(F("C"));
    display.setCursor(8, 57);
    display.print(maxTemperature, 1);
    display.println(F("C"));

    display.setCursor(50, 47);
    display.print(minHumidity, 1);
    display.println(F("%"));
    display.setCursor(50, 57);
    display.print(maxHumidity, 1);
    display.println(F("%"));

    display.setCursor(90, 47);
    display.print(minPressure, 1);
    display.setCursor(90, 57);
    display.print(maxPressure, 1);
  } else if (currentPage == 6) {
    display.setTextSize(1);

    display.setCursor(10, 47);
    display.print(minGasResistance, 1);
    display.println(F("KOhms"));
    display.setCursor(10, 57);
    display.print(maxGasResistance, 1);
    display.println(F("KOhms"));

    display.setCursor(75, 47);
    display.print(minIAQ, 1);
    display.println(F("IAQ"));
    display.setCursor(75, 57);
    display.print(maxIAQ, 1);
    display.println(F("IAQ"));
  } else if (currentPage == 7) {
    display.setCursor(55, 25);
    display.print(loadwatt);
    display.print(F("W"));
  } else if (currentPage == 8) {
    if (spotifyActivity == SPOTIFY_PLAYING) {
      display.clearDisplay();
      // display.fillTriangle(2, 8, 7, 3, 12, 8, WHITE);
      display.fillTriangle(0, 0, 4, 4, 0, 8, WHITE);
      if (appName == BT_AUDIO) {
        display.drawBitmap((display.width() / 2) - (youtubeLogoW / 2), 0, youtubeLogo, youtubeLogoW, youtubeLogoH, 1);
      } else {
        display.drawBitmap((display.width() / 2) - (spotifyLogoW / 2), 0, spotifyLogo, spotifyLogoW, spotifyLogoH, 1);
      }
      display.setTextSize(2);
      display.setTextWrap(false);

      // 12 is the text width
      int titleLen = mediaTitle.length() * 12;
      if (-titleLen > offset) {
        offset = 160;
      } else {
        offset -= 2;
      }
      display.setCursor(offset,spotifyLogoW + 5);

      display.println(mediaTitle);

      // 6 is the text width
      int authorLen = mediaArtist.length() * 6;
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
      display.setCursor(offsetAuthor, 47);
      display.println(mediaArtist);

      // float roundedVolumeLevel = volumeLevel.toFloat();
      // int volume = (roundedVolumeLevel > 0.99) ? display.width() : ((roundedVolumeLevel*100)*1.28);
      // draw position bar
      float currentMediaDuration = mediaDuration.toFloat();
      float currentMediaPosition = mediaPosition.toFloat();
      int position = (((currentMediaPosition * 100) / currentMediaDuration) * 1.28);
      display.drawRect(0, (display.height() - 4), display.width(), 4, WHITE);
      display.fillRect(0, (display.height() - 4), position, 4, WHITE);
    }
  } else if (currentPage == numPages) {
    bootstrapManager.drawInfoPage(VERSION, AUTHOR);
  }
  display.setTextWrap(true);

  if (pir == ON_CMD && currentPage != numPages) {
    // display.fillCircle(124, 13, 2, WHITE);
    if (currentPage == 8) {
      display.drawBitmap(117, 0, runLogo, runLogoW, runLogoH, 1);
    } else {
      display.drawBitmap(117, 12, runLogo, runLogoW, runLogoH, 1);
    }
  }

  bootstrapManager.drawScreenSaver("DPsoftware domotics");

  yield();

  if (furnanceTriggered) {
    drawCenterScreenLogo(furnanceTriggered, fireLogo, fireLogoW, fireLogoH, DELAY_4000);
  }

  if (acTriggered) {
    drawCenterScreenLogo(acTriggered, snowLogo, snowLogoW, snowLogoH, DELAY_4000);
  }

  if (showHaSplashScreen) {
    drawCenterScreenLogo(showHaSplashScreen, HABIGLOGO, HABIGLOGOW, HABIGLOGOH, DELAY_4000);
  }
  yield();

  if ((ssTriggered || (ssTriggerCycle > 0)) && !wpTriggered) {
    drawSolarStationTrigger(SOLAR_STATION_LOGO, SOLAR_STATION_LOGO_W, SOLAR_STATION_LOGO_H);
  }

  if (wpTriggered) {
    if (solarStationRemainingSeconds == "0") {
      wpTriggered = false;
    }
    drawWPRemainingSeconds(WATER_PUMP_LOGO, WATER_PUMP_LOGO_W, WATER_PUMP_LOGO_H);
  }

  if (temperature != OFF_CMD) {
    display.display();
  }

  /*Serial.print(F("Temp: "); Serial.print(temperature); Serial.println(F("°C");
  Serial.print(F("Humidity: "); Serial.print(humidity); Serial.println(F("%");
  Serial.print(F("Pressure: "); Serial.print(pressure); Serial.println(F("hPa");

  Serial.print(F("Caldaia: "); Serial.println(furnance);
  Serial.print(F("ac: "); Serial.println(ac);
  Serial.print(F("PIR: "); Serial.println(pir);

  Serial.print(F("target_temperature: "); Serial.println(target_temperature);
  Serial.print(F("hvac_action: "); Serial.println(hvac_action);
  Serial.print(F("away_mode: "); Serial.println(away_mode);
  Serial.print(F("alarmo: "); Serial.println(alarmo);*/
}

void drawHeader() {
  display.setTextSize(1);
  display.setCursor(0, 0);

  display.print(date);
  display.print(F("      "));
  display.print(currentime);

  display.drawLine(0, 8, display.width() - 1, 8, WHITE);

  hours = timedate.substring(11, 13);
  minutes = timedate.substring(14, 16);
}

void manageFooter() {
  if (switchFooter < (SWITCHFOOTEREVERYNDRAW / 2)) {
    drawFooterThermostat();
  } else {
    drawFooterSolarStation();
  }
  if (switchFooter > SWITCHFOOTEREVERYNDRAW) {
    switchFooter = 0;
  }
  switchFooter++;
}

void drawFooterThermostat() {
  display.setTextSize(1);
  display.setCursor(0, 57);
  display.print(target_temperature);
  (target_temperature != OFF_CMD) ? display.print(F("C")) : display.print(F(""));
  display.print(F(" "));
  display.print(humidity);
  display.print(F("%"));
  display.print(F(" "));
  display.print(pressure);
  display.print(F("hPa"));
}

void drawFooterSolarStation() {
  display.setTextSize(1);
  display.setCursor(0, 57);
  display.print(solarStationBatteryVoltage + "V  - " + solarStationBattery + " -  " + solarStationWifi + "%");
  // "4.2V  -  1012  -  46%");
}

void drawUpsFooter() {
  display.setTextSize(1);
  display.setCursor(0, 57);
  display.print(producing);
  display.print(F("FPS"));
  display.print(F(" "));
  display.print(consuming);
  display.print(F("FPS"));
  display.print(F(" "));
  display.print(gwconsuming);
  display.print(F("FPS"));
}

void drawCenterScreenLogo(bool &triggerBool, const unsigned char *logo, const int logoW, const int logoH,
                          const int delayInt) {
#if defined(ESP8266)
  ESP.wdtFeed();
#else
  esp_task_wdt_reset();
#endif
  drawCenterScreenLogo(logo, logoW, logoH);
  if (delayInt > 0) {
    delay(delayInt);
  }
  triggerBool = false;
}

void drawCenterScreenLogo(const unsigned char *logo, const int logoW, const int logoH) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.drawBitmap(
    (display.width() - logoW) / 2,
    (display.height() - logoH) / 2,
    logo, logoW, logoH, 1);
  display.display();
}

void drawSolarStationTrigger(const unsigned char *logo, const int logoW, const int logoH) {
  drawCenterScreenLogo(logo, logoW, logoH);
  ssTriggerCycle--;
}

void drawWPRemainingSeconds(const unsigned char *logo, const int logoW, const int logoH) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.drawBitmap(
    20,
    (display.height() - logoH) / 2,
    logo, logoW, logoH, 1);
  display.setCursor(70, 22);
  display.setTextSize(3);
  display.print(solarStationRemainingSeconds);
  display.display();
}

void drawRoundRect() {
  display.drawRoundRect(47, 19, 72, 27, 10, WHITE);
  display.drawRoundRect(47, 20, 71, 25, 9, WHITE);
  display.drawRoundRect(47, 20, 71, 25, 10, WHITE);
  display.drawRoundRect(47, 20, 71, 25, 8, WHITE);
  display.drawRoundRect(48, 20, 70, 25, 10, WHITE);
}

/********************************** START PROCESS JSON*****************************************/
bool processUpsStateJson(JsonDocument json) {
  if (json["runtime"].is<JsonVariant>()) {
    float loadFloat = json["load"];
#if defined(TARGET_SMARTOLED) || defined(TARGET_SMARTOLED_ESP32)
    if (loadFloat > HIGH_WATT && loadFloatPrevious < HIGH_WATT) {
      currentPage = 7;
    }
#endif
    loadFloatPrevious = loadFloat;
    if (loadwattMax < loadFloat) {
      loadwattMax = loadFloat;
    }

    loadwatt = serialized(String(loadFloat, 0));
    runtime = helper.getValue(json["runtime"]);
    float ivFloat = json["iv"];
    inputVoltage = serialized(String(ivFloat, 0));
    float ovFloat = json["ov"];
    outputVoltage = serialized(String(ovFloat, 0));
  }

  return true;
}

bool processSmartostatSensorJson(JsonDocument json) {
  if (json["BME680"].is<JsonVariant>()) {
    float temperatureFloat = json["BME680"]["Temperature"];
    temperature = serialized(String(temperatureFloat, 1));
    minTemperature = (temperatureFloat < minTemperature) ? temperatureFloat : minTemperature;
    maxTemperature = (temperatureFloat > maxTemperature) ? temperatureFloat : maxTemperature;
    float humidityFloat = json["BME680"]["Humidity"];
    humidity = serialized(String(humidityFloat, 1));
    minHumidity = (humidityFloat < minHumidity) ? humidityFloat : minHumidity;
    maxHumidity = (humidityFloat > maxHumidity) ? humidityFloat : maxHumidity;
    float pressureFloat = json["BME680"]["Pressure"];
    pressure = serialized(String(pressureFloat, 1));
    minPressure = (pressureFloat < minPressure) ? pressureFloat : minPressure;
    maxPressure = (pressureFloat > maxPressure) ? pressureFloat : maxPressure;
    float gasResistanceFloat = json["BME680"]["GasResistance"];
    gasResistance = serialized(String(gasResistanceFloat, 1));
    minGasResistance = (gasResistanceFloat < minGasResistance) ? gasResistanceFloat : minGasResistance;
    maxGasResistance = (gasResistanceFloat > maxGasResistance) ? gasResistanceFloat : maxGasResistance;
    float IAQFloat = json["BME680"]["IAQ"];
    IAQ = serialized(String(IAQFloat, 1));
    minIAQ = (IAQFloat < minIAQ) ? IAQFloat : minIAQ;
    maxIAQ = (IAQFloat > maxIAQ) ? IAQFloat : maxIAQ;
  }

  return true;
}

bool processSmartostatClimateJson(JsonDocument json) {
  String timeConst = json["Time"];
  // On first boot the timedate variable is OFF
  if (timedate == OFF_CMD) {
    helper.setDateTime(timeConst);
    lastBoot = date + " " + currentime;
  } else {
    helper.setDateTime(timeConst);
  }
  // lastMQTTConnection and lastWIFiConnection are resetted on every disconnection
  if (lastMQTTConnection == OFF_CMD) {
    lastMQTTConnection = date + " " + currentime;
  }
  if (lastWIFiConnection == OFF_CMD) {
    lastWIFiConnection = date + " " + currentime;
  }
  // reset min max
  // if (hours == "23" && minutes == "59") {
  //   resetMinMaxValues();
  // }
  haVersion = helper.getValue(json["haVersion"]);
  humidityThreshold = json["humidity_threshold"];
  tempSensorOffset = json["temp_sensor_offset"];
  int brightness = json["brightness"];

  display.ssd1306_command(0x81);
  display.ssd1306_command(brightness); //min 10 max 255
  display.ssd1306_command(0xD9);
  if (brightness <= 80) {
    display.ssd1306_command(31);
  } else {
    display.ssd1306_command(34);
  }

  String operationModeHeatConst = helper.getValue(json["smartostat"]["hvac_action"]);
  String operationModeCoolConst = helper.getValue(json["smartostatac"]["hvac_action"]);

  alarmo = helper.getValue(json["smartostat"]["alarmo"]);
  fan = helper.getValue(json["smartostatac"]["fan"]);

  if (operationModeHeatConst == HEAT || operationModeHeatConst == IDLE) {
    float target_temperatureFloat = json["smartostat"]["temperature"];
    target_temperature = serialized(String(target_temperatureFloat, 1));
    hvac_action = HEAT;
    const char *awayModeConst = json["smartostat"]["preset_mode"];
    away_mode = (strcmp(awayModeConst, "away") == 0) ? ON_CMD : OFF_CMD;
  } else if (operationModeCoolConst == COOL || operationModeCoolConst == IDLE) {
    float target_temperatureFloat = json["smartostatac"]["temperature"];
    target_temperature = serialized(String(target_temperatureFloat, 1));
    hvac_action = COOL;
    const char *awayModeConst = json["smartostatac"]["preset_mode"];
    away_mode = (strcmp(awayModeConst, "away") == 0) ? ON_CMD : OFF_CMD;
  } else {
    if (temperature.toFloat() > HEAT_COOL_THRESHOLD) {
      float target_temperatureFloat = json["smartostatac"]["temperature"];
      target_temperature = serialized(String(target_temperatureFloat, 1));
    } else {
      float target_temperatureFloat = json["smartostat"]["temperature"];
      target_temperature = serialized(String(target_temperatureFloat, 1));
    }
    hvac_action = OFF_CMD;
    away_mode = OFF_CMD;
  }

  return true;
}

bool processSpotifyStateJson(JsonDocument json) {
  //serializeJsonPretty(json, Serial); Serial.println();
  if (json["media_artist"].is<JsonVariant>()) {
    spotifyActivity = helper.getValue(json["spotify_activity"]);
    mediaTitle = helper.getValue(json["media_title"]);
    spotifySource = helper.getValue(json["spotifySource"]);
    volumeLevel = helper.getValue(json["volume_level"]);
    mediaDuration = helper.getValue(json["media_duration"]);
    mediaPosition = helper.getValue(json["media_position"]);
    mediaArtist = helper.getValue(json["media_artist"]);
    appName = helper.getValue(json["app_name"]);
    spotifyPosition = helper.getValue(json["position"]);

    if (appName != BT_AUDIO) {
      if ((spotifyActivity == SPOTIFY_PAUSED || spotifyActivity == SPOTIFY_IDLE) && mediaTitle == mediaTitlePrevious) {
        cleanSpotifyInfo();
      }
      if ((mediaTitle != mediaTitlePrevious) && (mediaTitlePrevious != OFF_CMD) && (mediaTitle != BT_AUDIO)) {
        currentPage = 8;
      }
    } else if (spotifyPosition != spotifyPositionPrevious) {
      spotifyActivity = SPOTIFY_PLAYING;
      if (mediaTitle != mediaTitlePrevious && (mediaTitle != BT_AUDIO)) {
        currentPage = 8;
      }
    } else {
      spotifyActivity = SPOTIFY_IDLE;
      cleanSpotifyInfo();
    }

    mediaTitlePrevious = helper.getValue(json["media_title"]);
    spotifyPositionPrevious = helper.getValue(json["position"]);
  }
  return true;
}

void cleanSpotifyInfo() {
  mediaTitle = EMPTY_STR;
  mediaArtist = EMPTY_STR;
  spotifySource = EMPTY_STR;
  volumeLevel = EMPTY_STR;
  mediaDuration = EMPTY_STR;
  mediaPosition = EMPTY_STR;
  appName = EMPTY_STR;
}

bool processSolarStationPowerState(JsonDocument json) {
  String solarStation = json["state"];
  if (solarStation == ON_CMD && stateOn) {
    ssTriggerCycle = 100;
    ssTriggered = true;
    stateOn = true;
    sendPowerState();
  } else {
    ssTriggered = false;
  }
  return true;
}

bool processSolarStationWaterPump(JsonDocument json) {
  String waterPump = helper.isOnOff(json);
  if (waterPump == ON_CMD) {
    wpTriggered = true;
    stateOn = true;
    sendPowerState();
  } else {
    wpTriggered = false;
  }
  return true;
}

bool processSolarStationState(JsonDocument json) {
  solarStationBattery = json["battery"];
  float voltage = ((solarStationBattery * 4.14) / 1024);
  solarStationBatteryVoltage = (serialized(String(voltage, 2)));
  solarStationWifi = helper.getValue(json["wifi"]);
  return true;
}

bool processSolarStationRemainingSeconds(JsonDocument json) {
  solarStationRemainingSeconds = helper.getValue(json["remaining_seconds"]);
  return true;
}

bool processSmartostatPirState(JsonDocument json) {
  pir = helper.isOnOff(json);
  return true;
}

bool processSmartoledCmnd(JsonDocument json) {
  String message = helper.isOnOff(json);
  if (message == ON_CMD) {
    stateOn = true;
  } else if (message == OFF_CMD) {
    stateOn = false;
  }
  screenSaverTriggered = false;
  sendPowerState();
  return true;
}

bool processSmartostatAcJson(JsonDocument json) {
  ac = helper.isOnOff(json);

  if (ac == ON_CMD) {
    acTriggered = true;
    stateOn = true;
    sendPowerState();
  }
  return true;
}

bool processFurnancedCmnd(JsonDocument json) {
  furnance = helper.isOnOff(json);
  if (furnance == ON_CMD) {
    furnanceTriggered = true;
    stateOn = true;
    sendPowerState();
  }
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
  sendFurnanceState();
  releManagement();
#endif
  return true;
}

bool processIrRecev(JsonDocument json) {
  irReceiveActive = (helper.isOnOff(json) == ON_CMD);
  return true;
}

// IRSEND MQTT message ON OFF only for Smartostat
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
bool toggleBeep(JsonDocument json) {
#if defined(ESP8266)
  ESP.wdtFeed();
#else
  esp_task_wdt_reset();
#endif
  acir.stateReset();
  acir.setBeep(true);
  acir.off();
  acir.send();
  acir.stateReset();
#if defined(ESP8266)
  EspClass::restart();
#else
  ESP.restart();
#endif
  return true;
}

bool processSmartostatRebootCmnd(JsonDocument json) {
  String msg = json[VALUE];
  String rebootState = msg;
  sendSmartostatRebootState(OFF_CMD);
  if (rebootState == OFF_CMD) {
    forceFurnanceOn = false;
    furnance = OFF_CMD;
    sendFurnanceState();
    forceACOn = false;
    ac = OFF_CMD;
    sendACState();
    bootstrapManager.publish(SMARTOSTAT_PIR_STATE_TOPIC, helper.string2char(OFF_CMD), true);
    releManagement();
    acManagement();
    sendSmartostatRebootCmnd();
  }
  return true;
}

bool processIrOnOffCmnd(JsonDocument json) {
  yield();

  String msg = json[VALUE];
  String acState = msg;
  if (acState == ON_CMD && ac == OFF_CMD) {
    acTriggered = true;
    ac = ON_CMD;
#if defined(ESP8266)
    ESP.wdtFeed();
#else
    esp_task_wdt_reset();
#endif
    acir.on();
    acir.setFan(kSamsungAcFanLow);
    acir.setMode(kSamsungAcCool);
    acir.setTemp(20);
    acir.setSwing(false);
    acir.sendExtended(IR_RETRY);
    sendACState();
  } else if (acState == OFF_CMD) {
    // set power mode before shutdown, if you don't do this sometimes the off command does not work
#if defined(ESP8266)
    ESP.wdtFeed();
#else
    esp_task_wdt_reset();
#endif
    if (stateOn) {
      // ac = ON_CMD;
      // acir.on();
      // acir.setMode(kSamsungAcCool);
      // acir.setTemp(28);
      // acir.setPowerful(true);
      // acir.setSwing(false);
      // acir.setQuiet(false);
      // acir.send(IR_RETRY);
      // delay(200);
    }
    ac = OFF_CMD;
    acir.stateReset();
    acir.off();
    yield();
    acir.sendOff(IR_RETRY);
    delay(200);
    sendACState();
  }
  //Serial.printf("  %s\n", acir.toString().c_str());
  return true;
}

bool processIrSendCmnd(JsonDocument json) {
  acir.on();
  if (json["alette_ac"].is<JsonVariant>()) {
    acir.setMode(kSamsungAcCool);

    const char *tempConst = json["temp"];
    uint8_t tempInt = atoi(tempConst);

    acir.setTemp(tempInt);

    String alette = json["alette_ac"];
    acir.setQuiet(false);
    acir.setPowerful(false);
    if (alette == off_CMD) {
      acir.setSwing(false);
      String modeConst = json["mode"];
      if (FAN_LOW == modeConst) {
        acir.setFan(kSamsungAcFanLow);
      } else if (FAN_POWER == modeConst) {
        acir.setPowerful(true);
      } else if (FAN_QUIET == modeConst) {
        acir.setQuiet(true);
      } else if (FAN_AUTO == modeConst) {
        acir.setFan(kSamsungAcFanAuto);
      } else if (FAN_HIGH == modeConst) {
        acir.setFan(kSamsungAcFanHigh);
      } else if (FAN_WARM == modeConst) {
        acir.setMode(kSamsungAcHeat);
        acir.setFan(kSamsungAcFanHigh);
      }
    } else {
      acir.setFan(kSamsungAcFanHigh);
      acir.setSwing(true);
    }
    acir.send(IR_RETRY);
    //Serial.printf("  %s\n", acir.toString().c_str());
  }
  return true;
}
#endif

#if defined(TARGET_SMARTOLED) || defined(TARGET_SMARTOLED_ESP32)

bool processSmartoledRebootCmnd(JsonDocument json) {
  rebootState = helper.isOnOff(json);
  sendSmartoledRebootState(OFF_CMD);
  if (rebootState == OFF_CMD) {
    sendSmartoledRebootCmnd();
  }
  return true;
}

bool processSmartoledFramerate(JsonDocument json) {
  if (json["producing"].is<JsonVariant>()) {
    float producingFloat = json["producing"];
    float consumingFloat = json["consuming"];
    producing = serialized(String(producingFloat, 1));
    consuming = serialized(String(consumingFloat, 1));
    if (producingFloat < 2) {
      gwconsuming = serialized(String(0, 1));;
    }
  }
  return true;
}

bool processSmartoledGlowWormFramerate(JsonDocument json) {
  if (json["framerate"].is<JsonVariant>()) {
    float consumingFloat = json["framerate"];
    gwconsuming = serialized(String(consumingFloat, 1));
  }
  return true;
}

bool processSmartostatFurnanceState(JsonDocument json) {
  furnance = helper.isOnOff(json);
  return true;
}

bool processACState(JsonDocument json) {
  ac = helper.isOnOff(json);
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

/********************************** SEND STATE *****************************************/
void sendPowerState() {
  bootstrapManager.publish(SMARTOLED_STATE_TOPIC, (stateOn) ? helper.string2char(ON_CMD) : helper.string2char(OFF_CMD),
                           true);
}

void sendInfoState() {
  JsonObject root = bootstrapManager.getJsonObject();
  root["State"] = (stateOn) ? ON_CMD : OFF_CMD;
  bootstrapManager.sendState(SMARTOLED_INFO_TOPIC, root, VERSION);
}


// Send PIR state via MQTT
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)

void sendSmartostatRebootState(String onOff) {
  bootstrapManager.publish(SMARTOSTAT_STAT_REBOOT, helper.string2char(onOff), true);
}

void sendSmartostatRebootCmnd() {
  delay(DELAY_1500);
  ESP.restart();
}

void sendPirState() {
  bootstrapManager.publish(SMARTOSTAT_PIR_STATE_TOPIC,
                           (pir == ON_CMD) ? helper.string2char(ON_CMD) : helper.string2char(OFF_CMD), true);
}

void sendSensorState() {
  JsonObject root = bootstrapManager.getJsonObject();

  root["Time"] = timedate;
  root["state"] = (stateOn) ? ON_CMD : OFF_CMD;
  root["POWER1"] = furnance;
  root["POWER2"] = pir;

  JsonObject BME680 = root["BME680"].to<JsonObject>();
  if (readOnceEveryNTimess == 0 && sensorOk) {
    if (!boschBME680.performReading()) {
      Serial.println("Failed to perform reading :(");
      delay(500);
      return;
    }
    BME680["Temperature"] = serialized(String(boschBME680.temperature + tempSensorOffset, 1));
    BME680["Humidity"] = serialized(String(boschBME680.humidity, 1));
    BME680["Pressure"] = boschBME680.pressure / 100;
    BME680["GasResistance"] = serialized(String(gas_reference / 1000, 1));
    humidity_score = getHumidityScore();
    gas_score = getGasScore();
    //Combine results for the final IAQ index value (0-100% where 100% is good quality air)
    float air_quality_score = humidity_score + gas_score;
    if ((getgasreference_count++) % 5 == 0) getGasReference();
    BME680["IAQ"] = calculateIAQ(air_quality_score);
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

  bootstrapManager.publish(SMARTOSTAT_SENSOR_STATE_TOPIC, root, true);
}

void sendFurnanceState() {
  if (furnance == OFF_CMD) {
    forceFurnanceOn = false;
  }
  bootstrapManager.publish(SMARTOSTAT_FURNANCE_STATE_TOPIC,
                           (furnance == OFF_CMD) ? helper.string2char(OFF_CMD) : helper.string2char(ON_CMD), true);
}
#endif

#if defined(TARGET_SMARTOLED) || defined(TARGET_SMARTOLED_ESP32)

void sendSmartoledRebootState(String onOff) {
  bootstrapManager.publish(SMARTOLED_STAT_REBOOT, helper.string2char(onOff), true);
}

void sendSmartoledRebootCmnd() {
  delay(DELAY_1500);
  ESP.restart();
}

#endif

void sendACCommandState() {
  if (ac == OFF_CMD) {
    forceACOn = false;
  }
  bootstrapManager.publish(SMARTOSTATAC_CMND_IRSENDSTATE,
                           (ac == OFF_CMD) ? helper.string2char(OFF_CMD) : helper.string2char(ON_CMD), true);
}

void sendClimateState(String mode) {
  if (mode == COOL) {
    bootstrapManager.publish(SMARTOSTAT_CMND_CLIMATE_COOL_STATE,
                             (ac == OFF_CMD) ? helper.string2char(OFF_CMD) : helper.string2char(ON_CMD), true);
  } else {
    bootstrapManager.publish(SMARTOSTAT_CMND_CLIMATE_HEAT_STATE,
                             (furnance == OFF_CMD) ? helper.string2char(OFF_CMD) : helper.string2char(ON_CMD), true);
  }
}

void sendFurnanceCommandState() {
  if (furnance == OFF_CMD) {
    forceFurnanceOn = false;
  }
  bootstrapManager.publish(SMARTOSTAT_FURNANCE_CMND_TOPIC,
                           (furnance == OFF_CMD) ? helper.string2char(OFF_CMD) : helper.string2char(ON_CMD), true);
}

void sendACState() {
  if (ac == OFF_CMD) {
    forceACOn = false;
  }
  bootstrapManager.publish(SMARTOSTATAC_STAT_IRSEND,
                           (ac == OFF_CMD) ? helper.string2char(OFF_CMD) : helper.string2char(ON_CMD), true);
}

// Send status to MQTT broker every ten seconds
void delayAndSendStatus() {
  if (millis() > timeNowStatus + tenSecondsPeriod) {
    timeNowStatus = millis();
    ledTriggered = true;
    sendPowerState();
#if defined(ESP8266)
    ESP.wdtFeed();
#endif
    sendInfoState();
#if defined(ESP8266)
    ESP.wdtFeed();
#endif
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
    sendSensorState();
#if defined(ESP8266)
    ESP.wdtFeed();
#endif
    sendFurnanceState();
#if defined(ESP8266)
    ESP.wdtFeed();
#endif
    sendACState();
#if defined(ESP8266)
    ESP.wdtFeed();
#endif
#endif
  }
}

// Go to home page after five minutes of inactivity and write SPIFFS
void goToHomePageAndWriteToStorageAfterFiveMinutes() {
  if (millis() > timeNowGoHomeAfterFiveMinutes + fiveMinutesPeriod) {
    timeNowGoHomeAfterFiveMinutes = millis();
    // Ping gateway to add presence on the routing table,
    // command is synchrounous and adds a bit of lag to the loop
#if defined(ESP8266)
    pingESP.ping();
#endif
    // Write data to file system
    writeConfigToStorage();
    screenSaverTriggered = true;
    if ((humidity != OFF_CMD && humidity.toFloat() < humidityThreshold) && (loadFloatPrevious < HIGH_WATT) && (
          (spotifyActivity == SPOTIFY_PLAYING && currentPage != 8) || spotifyActivity != SPOTIFY_PLAYING)) {
      currentPage = 0;
    }
  }
}

/********************************** PIR, RELE' AND IR MANAGEMENT *****************************************/
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)

void pirManagement() {
  if (digitalRead(SR501_PIR_PIN) == HIGH) {
    if (pir == OFF_CMD) {
      highIn = millis();
      pir = ON_CMD;
    }
    if (pir == ON_CMD) {
      if ((millis() - highIn) > 500) {
        // 7000 four seconds on time
        if (lastPirState != ON_CMD) {
          lastPirState = ON_CMD;
          sendPirState();
        }
        highIn = millis();
      }
    }
  }
  if (digitalRead(SR501_PIR_PIN) == LOW) {
    highIn = millis();
    if (pir == ON_CMD) {
      pir = OFF_CMD;
      if (lastPirState != OFF_CMD) {
        lastPirState = OFF_CMD;
        sendPirState();
      }
    }
  }
}

void releManagement() {
  if (furnance == ON_CMD || forceFurnanceOn) {
    digitalWrite(RELE_PIN, HIGH);
  } else {
    digitalWrite(RELE_PIN, LOW);
  }
}

void acManagement() {
  if (ac == ON_CMD || forceACOn) {
    acir.on();
    acir.setFan(kSamsungAcFanLow);
    acir.setMode(kSamsungAcCool);
    acir.setTemp(20);
    acir.setSwing(false);
    acir.sendExtended(IR_RETRY);
  } else {
    acir.off();
    acir.sendOff(IR_RETRY);
  }
}

void getGasReference() {
  // Now run the sensor for a burn-in period, then use combination of relative humidity and gas resistance to estimate indoor air quality as a percentage.
  //Serial.println("Getting a new gas reference value");
  int readings = 10;
  for (int i = 1; i <= readings; i++) {
    yield();
#if defined(ESP8266)
    ESP.wdtFeed();
#else
    esp_task_wdt_reset();
#endif
    // read gas for 10 x 0.150mS = 1.5secs
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

int getHumidityScore() {
  //Calculate humidity contribution to IAQ index
  float current_humidity = boschBME680.humidity;
  if (current_humidity >= 38 && current_humidity <= 42) // Humidity +/-5% around optimum
    humidity_score = 0.25 * 100;
  else {
    // Humidity is sub-optimal
    if (current_humidity < 38)
      humidity_score = 0.25 / hum_reference * current_humidity * 100;
    else {
      humidity_score = ((-0.25 / (100 - hum_reference) * current_humidity) + 0.416666) * 100;
    }
  }
  return humidity_score;
}

int getGasScore() {
  //Calculate gas contribution to IAQ index
  gas_score = (0.75 / (gas_upper_limit - gas_lower_limit) * gas_reference - (
                 gas_lower_limit * (0.75 / (gas_upper_limit - gas_lower_limit)))) * 100.00;
  if (gas_score > 75) gas_score = 75; // Sometimes gas readings can go outside of expected scale maximum
  if (gas_score < 0) gas_score = 0; // Sometimes gas readings can go outside of expected scale minimum
  return gas_score;
}

void manageIrRecv() {
  // Check if the IR code has been received.
  if (irrecv.decode(&results)) {
    // Check if we got an IR message that was to big for our capture buffer.
    if (results.overflow) {
      bootstrapManager.publish(IR_RECV_TOPIC, "MSG TOO BIG FOR THE BUFFER", false);
    }
    // Display the basic output of what we found.
    bootstrapManager.publish(IR_RECV_TOPIC, helper.string2char(resultToHumanReadableBasic(&results)), false);
    // Display any extra A/C info if we have it.
    String description = IRAcUtils::resultAcToString(&results);
    if (description.length()) {
      bootstrapManager.publish(IR_RECV_TOPIC, helper.string2char(D_STR_MESGDESC ": " + description), false);
    }
    yield(); // Feed the WDT as the text output can take a while to print.
    // Output the results as source code
    String srcCode = resultToSourceCode(&results);
    int chunkSize = 900;
    for (unsigned i = 0; i < srcCode.length(); i += chunkSize) {
      if (i + chunkSize < srcCode.length()) {
        bootstrapManager.publish(IR_RECV_TOPIC, helper.string2char(srcCode.substring(i, i + chunkSize)), false);
      } else {
        bootstrapManager.publish(IR_RECV_TOPIC, helper.string2char(srcCode.substring(i, srcCode.length())), false);
      }
      delay(DELAY_500);
    }
    yield(); // Feed the WDT (again)
  }
}

#endif

/********************************** TOUCH BUTTON MANAGEMENT *****************************************/
void touchButtonManagement(int digitalReadButtonPin) {
  buttonState = digitalReadButtonPin;
  // Quick presses
  if (buttonState == HIGH && lastReading == LOW) {
    // function triggered on the quick press of the button
    onTime = millis();
    pressed = true;
    quickPress();
  } else if (buttonState == LOW && pressed) {
    // function triggered on the release of the button
    pressed = false;
    if (longPress) {
      // button release long pressed
      longPress = false;
      longPressRelease();
    } else if (veryLongPress) {
      veryLongPress = false;
      veryLongPressRelease();
    } else {
      // button release no long press
      quickPressRelease();
    }
  }
  // Long press for a second
  if (buttonState == HIGH && lastReading == HIGH) {
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
    if ((millis() - onTime) > 4000) {
      // a second hold time
      lastReading = LOW;
      veryLongPress = true;
    }
#endif
#if defined(TARGET_SMARTOLED) || defined(TARGET_SMARTOLED_ESP32)
    if (((millis() - onTime) > 4000)) {
      // a second hold time
      lastReading = LOW;
      longPress = false;
      veryLongPress = true;
    } else if (((millis() - onTime) > 1000)) {
      // a second hold time
      lastReading = LOW;
      longPress = true;
      veryLongPress = false;
    }
#endif
  }
  lastReading = buttonState;
}

void longPressRelease() {
  commandButtonRelease();
}

void veryLongPressRelease() {
  // turn off the display on long pressed
  stateOn = false;
  resetMinMaxValues();
  writeConfigToStorage();
}

void quickPress() {
  // reset the go to homepage timer on quick button press
  timeNowGoHomeAfterFiveMinutes = millis();
}

void commandButtonRelease() {
  if (temperature.toFloat() > HEAT_COOL_THRESHOLD) {
    if (ac == ON_CMD) {
      ac = OFF_CMD;
      // stop ac on long press if wifi or mqtt is disconnected
      forceACOn = false;
    } else {
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
      acTriggered = true;
#endif
      ac = ON_CMD;
      // start ac on long press if wifi or mqtt is disconnected
      forceACOn = true;
    }
    sendACCommandState();
    sendClimateState(COOL);
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
    acManagement();
#endif
    lastButtonPressed = OLED_BUTTON_PIN;
  } else {
    if (furnance == ON_CMD) {
      furnance = OFF_CMD;
      // stop furnance on long press if wifi or mqtt is disconnected
      forceFurnanceOn = false;
    } else {
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
      furnanceTriggered = true;
#endif
      furnance = ON_CMD;
      // start furnance on long press if wifi or mqtt is disconnected
      forceFurnanceOn = true;
    }
    sendFurnanceCommandState();
    sendClimateState(HEAT);
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
    releManagement();
#endif
    lastButtonPressed = OLED_BUTTON_PIN;
  }
}

void quickPressRelease() {
  // turn on the furnance
  if (lastButtonPressed == SMARTOSTAT_BUTTON_PIN) {
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
    commandButtonRelease();
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
void readConfigFromStorage() {
  JsonDocument doc;
  doc = bootstrapManager.readLittleFS("config.json");
  if (!(doc[VALUE].is<JsonVariant>() && doc[VALUE] == ERROR)) {
    Serial.println(F("\nReload previously stored values."));
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
  }
}

void writeConfigToStorage() {
  JsonDocument doc;
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
  bootstrapManager.writeToLittleFS(doc, "config.json");
}

/********************************** START MAIN LOOP *****************************************/
void loop() {
  // Bootsrap loop() with Wifi, MQTT and OTA functions
  bootstrapManager.bootstrapLoop(manageDisconnections, manageQueueSubscription, manageHardwareButton);

  if (irReceiveActive) {
    if (!printIrReceiving) {
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
      // Ignore messages with less than minimum on or off pulses. NOTE: Set this value very high to effectively turn off UNKNOWN detection.
      irrecv.setUnknownThreshold(12);
      irrecv.enableIRIn(); // Start the receiver
#endif
      drawCenterScreenLogo(showHaSplashScreen, IR_RECV_LOGO, IR_RECV_LOGO_W, IR_RECV_LOGO_H, 0);
      printIrReceiving = true;
    }
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
    manageIrRecv();
#endif
  } else {
    printIrReceiving = false;
#if defined(ESP8266)
    ESP.wdtFeed();
#endif
    // PIR and RELAY MANAGEMENT
#if defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
    manageHardwareButton();
    pirManagement();
#else
    lastButtonPressed = OLED_BUTTON_PIN;
    touchButtonManagement(digitalRead(OLED_BUTTON_PIN));
#endif

    // Draw Speed, it influences how long the button should be pressed before switching to the next currentPage
    delay(delayTime);

    // Send status on MQTT Broker every n seconds
    delayAndSendStatus();

    // DRAW THE SCREEN
#if defined(TARGET_SMARTOLED) || defined(TARGET_SMARTOLED_ESP32)
    if (stateOn || (loadFloat > HIGH_WATT)) {
#elif defined(TARGET_SMARTOSTAT) || defined(TARGET_SMARTOSTAT_ESP32)
    if (stateOn) {
#endif
      draw();
    } else {
      display.clearDisplay();
      display.display();
    }
#if defined(ESP8266)
    ESP.wdtFeed();
#endif
    // Go To Home Page timer after 5 minutes of inactivity and write data to File System (SPIFFS)
    goToHomePageAndWriteToStorageAfterFiveMinutes();

    // bootstrapManager.nonBlokingBlink();
#if defined(ESP8266)
    ESP.wdtFeed();
#endif
  }
}
