#ifndef alarm_setting_h
#define alarm_setting_h

#include <Arduino.h>


enum AlarmMode
{
    OFF,
    ON,
    WEEK_DAYS,
    WEEK_ENDS
};

enum AlarmSettingStage
{
    NONE,
    SET_HOUR,
    SET_MIN,
    SET_MODE
};

struct AlarmSetting
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


#endif
