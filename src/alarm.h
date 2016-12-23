#ifndef alarm_h
#define alarm_h

#include <Arduino.h>
#include <OLEDDisplayUi.h>
#include <TimeClient.h>
#include <WundergroundClient.h>

enum AlarmSettingStage
{
    NONE,
    SET_HOUR,
    SET_MIN,
    SET_MODE
};

enum AlarmMode
{
    OFF,
    ON,
    WEEK_DAYS,
    WEEK_ENDS
};

struct AlarmSettings
{
public:
    AlarmMode mode = OFF;
    String getModeString()
    {
        switch (mode)
        {
            case OFF:
                return "Off";
            case ON:
                return "On";
            case WEEK_DAYS:
                return "WEEKDAYS";
        }
    }

    int16_t hours = 0;
    String getFormatedHours()
    {
        if (hours < 10 )
        {
            return "0" + String(hours);
        }
        return String(hours);
    }

    int16_t minutes = 0;
    String getFormatedMinutes()
    {
        if (minutes < 10 )
        {
            return "0" + String(minutes);
        }
        return String(minutes);
    }

    String getFormatedTime()
    {
        String result = getFormatedHours() + ":";
        result += getFormatedMinutes();
        return result;
    }
};

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

    AlarmSettings getSettings();
    AlarmSettingStage getSettingStage();

    void onMinusButtonClick();
    void onPlusButtonClick();

private:
    TimeClient *timeClient;
    WundergroundClient *wunderground;
    AlarmSettings settings;
    AlarmSettingStage settingStage = NONE;

    int beeps = 0;
    long stopBeeps = 0;

    void loadSettings();
    void saveSettings();
};

#endif
