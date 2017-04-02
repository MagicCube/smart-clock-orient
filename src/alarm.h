#ifndef alarm_h
#define alarm_h

#include <Arduino.h>
#include <OLEDDisplayUi.h>
#include <TimeClient.h>
#include <WundergroundClient.h>

#include "alarm-setting.h";

class Alarm
{
public:
    bool beeping = false;

    void begin(WundergroundClient *wunderground, TimeClient *timeClient);
    void handle();
    void draw(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);

    void stopBeeping();

    void beginSetting();
    void endSetting();
    bool nextSettingStage();

    AlarmSetting getSettings();
    AlarmSettingStage getSettingStage();

    void onMinusButtonClick();
    void onPlusButtonClick();

private:
    TimeClient *timeClient;
    WundergroundClient *wunderground;
    AlarmSetting settings;
    AlarmSettingStage settingStage = NONE;

    int beeps = 0;
    long stopBeeps = 0;

    void loadSettings();
    void saveSettings();
};

#endif
