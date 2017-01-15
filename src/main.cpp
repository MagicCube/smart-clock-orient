#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <Wire.h>


#include <JsonListener.h>
#include <MovingAverageFilter.h>
#include <OLEDDisplayUi.h>
#include <SH1106Wire.h>
#include <Ticker.h>
#include <TimeClient.h>
#include <WundergroundClient.h>

#include "fonts.h"
#include "http-service.h"
#include "images.h"
#include "alarm.h"

// HTTP Service
HttpService service;

// Setup
const int UPDATE_INTERVAL_SECS = 60 * 60; // Update every 60 minutes

// Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;
const int SDA_PIN = D2;
const int SDC_PIN = D1;

// Button Settings
long lastButtonClick = 0;
const int PLUS_BUTTON_PIN = D5;
const int MIN_BUTTON_PIN = D6;
const int ENTER_BUTTON_PIN = D7;

// TimeClient Settings
const float UTC_OFFSET = 8;

// Wunderground Settings
const boolean IS_METRIC = true;
const String WUNDERGRROUND_API_KEY = "2d4a4e7587426081";
const String WUNDERGRROUND_LANGUAGE = "EN";
const String WUNDERGROUND_COUNTRY = "CN";
const String WUNDERGROUND_CITY = "Nanjing";

// Initialize the oled display for address
SH1106Wire display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
OLEDDisplayUi ui(&display);
bool autoTransition = true;

bool upgrading = false;
int upgradingProgress = 0;

long lastActiveTime = 0;
bool sleeping = false;

enum UIMode
{
    NORMAL,
    ALARM
};
UIMode uiMode = NORMAL;

Alarm alarm;

/***************************
   End Settings
 **************************/

TimeClient timeClient(UTC_OFFSET);

WundergroundClient wunderground(IS_METRIC);

// This flag changed in the ticker function every 60 minutes
bool readyForWeatherUpdate = false;

String lastUpdate = "--";
long lastTemperatureUpdate = 0;

Ticker ticker;

// Indoor
int temperature = INT32_MAX;
// Use MovingAverage to caculate the mean value of the temperature in the last 3
// minutes
const int TEMPERATURE_MA_POINT_COUNT = 3 * 60 / 10;
MovingAverageFilter temperatureMA(TEMPERATURE_MA_POINT_COUNT);




// Declaring prototypes
void drawProgress(OLEDDisplay *display, int percentage, String label);
void updateTemperature();
void updateData(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawIndoor(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState *state);
void drawAlarm(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void setReadyForWeatherUpdate();
void plusButton_click();
void minButton_click();
void enterButton_click();

// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback normalFrames[] = {drawDateTime, drawCurrentWeather, drawForecast};
int numberOfNormalFrames = 3;
FrameCallback alarmFrames[] = {drawAlarm};

OverlayCallback overlays[] = {drawHeaderOverlay};
int numberOfOverlays = 1;

void setup()
{
    Serial.begin(115200);
    Serial.println();
    Serial.println();

    // Initialize dispaly
    display.init();
    display.clear();
    display.flipScreenVertically();
    display.display();

    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setContrast(255);

    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 15, "SMART CLOCK");
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 35, "by Henry");
    display.display();
    delay(2000);

    WiFi.mode(WIFI_AP_STA);
    WiFi.mode(WIFI_STA);

    // Auto scan WiFi connection
    String prefSSID = "none";
    String prefPassword;
    int ssidCount = WiFi.scanNetworks();
    if (ssidCount == -1)
    {
        return;
    }
    for (int i = 0; i < ssidCount; i++)
    {
        String ssid = WiFi.SSID(i);
        if (ssid.equals("Henry's Living Room 2.4GHz"))
        {
            prefSSID = ssid;
            prefPassword = "13913954971";
            break;
        }
        else if (ssid.equals("Henry's TP-LINK"))
        {
            prefSSID = ssid;
            prefPassword = "13913954971";
            break;
        }
        else if (ssid.equals("Henry's iPhone 6"))
        {
            prefSSID = ssid;
            prefPassword = "13913954971";
            // Don't break, cause this will connect to 4G network.
            // It's absolutely not a first choise.
        }
    }
    if (prefSSID.equals("none"))
    {
        return;
    }
    WiFi.begin(prefSSID.c_str(), prefPassword.c_str());
    WiFi.softAP("Smart-Clock-Orient");

    // Setup OTA
    ArduinoOTA.begin();
    ArduinoOTA.onStart([]() {
        upgrading = true;
        upgradingProgress = 0;
        display.displayOn();
        display.setContrast(255);
        drawProgress(&display, 0, "Uploading firmware...");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        upgradingProgress = progress / (total / 100);
        drawProgress(&display, upgradingProgress, "Uploading firmware...");
    });
    ArduinoOTA.onEnd([]() {
        upgradingProgress = 100;
        drawProgress(&display, upgradingProgress, "Done");
        delay(500);
        drawProgress(&display, upgradingProgress, "Restarting...");
    });

    int counter = 0;
    display.setFont(ArialMT_Plain_10);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        display.clear();
        display.drawString(64, 10, "Connecting to WiFi");
        display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
        display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
        display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
        display.flipScreenVertically();
        display.display();

        counter++;
    }

    // Setup Alarm
    alarm.begin(&wunderground, &timeClient);

    // Start server immediately after WiFi connection established
    //service.begin();

    // Setup UI
    ui.setTargetFPS(30);
    ui.setActiveSymbol(activeSymbole);
    ui.setInactiveSymbol(inactiveSymbole);
    ui.setIndicatorPosition(BOTTOM);
    ui.setIndicatorDirection(LEFT_RIGHT);
    ui.setFrameAnimation(SLIDE_LEFT);
    ui.setFrames(normalFrames, numberOfNormalFrames);
    ui.setOverlays(overlays, numberOfOverlays);
    ui.setTimePerTransition(360);
    ui.setTimePerFrame(10 * 1000);
    ui.disableAutoTransition();
    ui.init();


    // Setup Buttons
    pinMode(PLUS_BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PLUS_BUTTON_PIN), plusButton_click, FALLING);
    pinMode(MIN_BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(MIN_BUTTON_PIN), minButton_click, FALLING);
    pinMode(ENTER_BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(ENTER_BUTTON_PIN), enterButton_click, FALLING);

    Serial.println("");

    updateData(&display);

    // Execute setReadyForWeatherUpdate() every UPDATE_INTERVAL_SECS seconds
    ticker.attach(UPDATE_INTERVAL_SECS, setReadyForWeatherUpdate);

    lastActiveTime = millis();
}

void loop()
{
    ArduinoOTA.handle();
    if (upgrading)
    {
        return;
    }

    if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED)
    {
        updateData(&display);
    }

    alarm.handle();

    if (alarm.beeping)
    {
        sleeping = false;
        display.displayOn();
        display.setContrast(255);
    }
    else if (millis() <= lastActiveTime + 10 * 1000)
    {
        sleeping = false;
        display.displayOn();
        display.setContrast(255);
    }
    else if (millis() <= lastActiveTime + 15 * 1000)
    {
        sleeping = false;
        display.displayOn();
        display.setContrast(128);
    }
    else
    {
        if (timeClient.hours() >= 22 || timeClient.hours() <= 6)
        {
            sleeping = true;
            display.displayOff();
            display.setContrast(0);
        }
        else
        {
            sleeping = false;
            display.displayOn();
            display.setContrast(0);
        }
    }

    int remainingTimeBudget = ui.update();
    if (remainingTimeBudget > 0)
    {
        delay(remainingTimeBudget);
    }

    /*
    if (lastTemperatureUpdate == 0 || millis() - lastTemperatureUpdate > 10 * 1000)
    {
        // Update temperature every 10 seconds
        updateTemperature();
    }
    */

    //service.loop();
}





void drawProgress(OLEDDisplay *display, int percentage, String label)
{
    display->clear();
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(ArialMT_Plain_10);
    display->drawString(64, 10, label);
    display->drawProgressBar(2, 28, 124, 10, percentage);
    display->flipScreenVertically();
    display->display();
}

void updateData(OLEDDisplay *display)
{
    drawProgress(display, 10, "Updating time...");
    timeClient.updateTime();
    drawProgress(display, 30, "Updating conditions...");
    wunderground.updateConditions(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
    drawProgress(display, 50, "Updating forecasts...");
    wunderground.updateForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
    //drawProgress(display, 80, "Updating temperature...");
    //updateTemperature();
    lastUpdate = timeClient.getFormattedTime();
    readyForWeatherUpdate = false;
    drawProgress(display, 100, "Done...");
    delay(100);
}

void updateTemperature()
{
    int value = analogRead(A0);
    float temp = (float)value / 1023 * 3 * 100;
    Serial.print("Temperature: ");
    Serial.print(temp);
    Serial.print(" / ");
    if (temperature == INT32_MAX)
    {
        for (int i = 0; i < TEMPERATURE_MA_POINT_COUNT; i++)
        {
            temperature = round(temperatureMA.process(temp));
        }
    }
    else
    {
        temperature = round(temperatureMA.process(temp));
    }
    service.setTemperature(temperature);
    lastTemperatureUpdate = millis();
    Serial.println(temperature);
}

void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(ArialMT_Plain_10);
    String date = wunderground.getDate();
    int textWidth = display->getStringWidth(date);
    display->drawString(64 + x, 7 + y, date);
    display->setFont(ArialMT_Plain_24);
    String time = timeClient.getFormattedTime();
    textWidth = display->getStringWidth(time);
    display->drawString(64 + x, 17 + y, time);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setFont(ArialMT_Plain_10);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(60 + x, 7 + y, wunderground.getWeatherText());

    display->setFont(ArialMT_Plain_24);
    String temp = wunderground.getCurrentTemp() + "°C";
    display->drawString(60 + x, 17 + y, temp);
    int tempWidth = display->getStringWidth(temp);

    display->setFont(Meteocons_Plain_42);
    String weatherIcon = wunderground.getTodayIcon();
    int weatherIconWidth = display->getStringWidth(weatherIcon);
    display->drawString(32 + x - weatherIconWidth / 2, 05 + y, weatherIcon);
}

void drawForecast(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    drawForecastDetails(display, x, y + 8, 0);
    drawForecastDetails(display, x + 44, y + 8, 2);
    drawForecastDetails(display, x + 88, y + 8, 4);
}

void drawIndoor(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(ArialMT_Plain_10);
    display->drawString(64 + x, 5 + y, "Indoor");
    display->setFont(ArialMT_Plain_24);
    if (temperature != INT32_MAX)
    {
        display->drawString(64 + x, 15 + y, String(temperature) + " °C");
    }
    else
    {
        display->drawString(64 + x, 15 + y, "-- °C");
    }
}

void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex)
{
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(ArialMT_Plain_10);
    String day = wunderground.getForecastTitle(dayIndex).substring(0, 3);
    day.toUpperCase();
    display->drawString(x + 20, y, day);

    display->setFont(Meteocons_Plain_21);
    display->drawString(x + 20, y + 11, wunderground.getForecastIcon(dayIndex));

    display->setFont(ArialMT_Plain_10);
    display->drawString(x + 20, y + 29, wunderground.getForecastLowTemp(dayIndex) + "|" +
                                            wunderground.getForecastHighTemp(dayIndex));
    display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
    display->setColor(WHITE);
    display->setFont(ArialMT_Plain_10);
    String time = timeClient.getFormattedTime().substring(0, 5);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(0, 54, time);
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    if (alarm.getSettings().mode != OFF)
    {
        int width = display->getStringWidth("ALARM");
        display->setColor(WHITE);
        display->fillRect(128 - width - 2, 56, width + 4, 15);
        display->setColor(BLACK);
        display->drawString(127, 54, "ALARM");
        display->setColor(WHITE);
    }
    //display->drawString(128, 54, wunderground.getCurrentTemp() + "°C");
    display->drawHorizontalLine(0, 53, 128);
}




void drawAlarm(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    alarm.draw(display, state, x, y);
}




void setReadyForWeatherUpdate()
{
    Serial.println("Setting readyForUpdate to true");
    readyForWeatherUpdate = true;
}





void enterButton_click()
{
    if (ui.getUiState()->frameState == IN_TRANSITION)
    {
        return;
    }
    if (lastButtonClick != 0 && millis() - lastButtonClick < 300)
    {
        lastButtonClick = millis();
        return;
    }
    lastButtonClick = millis();
    Serial.print("enter ");
    Serial.println(millis());
    lastActiveTime = millis();
    if (sleeping)
    {
        sleeping = false;
        return;
    }

    if (uiMode == NORMAL)
    {
        /*
        autoTransition = !autoTransition;
        if (autoTransition)
        {
            ui.enableAutoTransition();
            ui.disableAllIndicators();
        }
        else
        {
            ui.disableAutoTransition();
            ui.enableAllIndicators();
        }
        */
        if (alarm.beeping)
        {
            alarm.stopBeeping();
            return;
        }
        uiMode = ALARM;
        alarm.beginSetting();
        ui.setFrames(alarmFrames, 1);
        ui.disableAutoTransition();
        ui.disableAllIndicators();
    }
    else
    {
        bool hasNextStage = alarm.nextSettingStage();
        if (!hasNextStage)
        {
            uiMode = NORMAL;
            ui.setFrames(normalFrames, numberOfNormalFrames);
            //ui.enableAutoTransition();
            ui.enableAllIndicators();
        }
    }
}

void plusButton_click()
{
    if (ui.getUiState()->frameState == IN_TRANSITION)
    {
        return;
    }
    if (lastButtonClick != 0 && millis() - lastButtonClick < 200)
    {
        lastButtonClick = millis();
        return;
    }
    lastButtonClick = millis();
    Serial.print("+ ");
    Serial.println(millis());
    lastActiveTime = millis();
    if (sleeping)
    {
        sleeping = false;
        return;
    }


    if (uiMode == NORMAL)
    {
        if (alarm.beeping)
        {
            alarm.stopBeeping();
            return;
        }
        //ui.nextFrame();
        ui.previousFrame();
    }
    else if (uiMode == ALARM)
    {
        alarm.onPlusButtonClick();
    }
}

void minButton_click()
{
    if (ui.getUiState()->frameState == IN_TRANSITION)
    {
        return;
    }
    if (lastButtonClick != 0 && millis() - lastButtonClick < 200)
    {
        lastButtonClick = millis();
        return;
    }
    lastButtonClick = millis();
    Serial.print("- ");
    Serial.println(millis());
    lastActiveTime = millis();
    if (sleeping)
    {
        sleeping = false;
        return;
    }

    if (uiMode == NORMAL)
    {
        if (alarm.beeping)
        {
            alarm.stopBeeping();
            return;
        }

        //ui.previousFrame();
        ui.nextFrame();
    }
    else if (uiMode == ALARM)
    {
        alarm.onMinusButtonClick();
    }
}
