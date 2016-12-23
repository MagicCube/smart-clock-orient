#include <Arduino.h>
#include <EEPROM.h>
#include <OLEDDisplayUi.h>
#include <TimeClient.h>
#include <WundergroundClient.h>

#include "alarm.h"

#define DELTA_MIN 5
const int BUZZER_PIN = D8;
const int LED_PIN = D3;

void Alarm::begin(WundergroundClient *wunderground, TimeClient *timeClient)
{
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);

    EEPROM.begin(512);
    this->wunderground = wunderground;
    this->timeClient = timeClient;
    this->loadSettings();
}

void Alarm::handle()
{
    if (this->settings.mode == OFF || this->settingStage != NONE)
    {
        this->beeping = false;
        digitalWrite(BUZZER_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
        return;
    }
    String date = this->wunderground->getDate();
    String day = date.substring(0, 3);
    if (this->settings.mode == WEEK_DAYS && (day.equals("Sat") || (day.equals("Sun"))))
    {
        digitalWrite(BUZZER_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
        return;
    }
    if (this->stopBeeps + 60 * 1000 > millis())
    {
        this->beeping = false;
        digitalWrite(BUZZER_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
        return;
    }

    if (this->timeClient->hours() == this->settings.hours && (this->timeClient->minutes() == this->settings.minutes))
    {
        this->beeping = true;
        digitalWrite(LED_PIN, HIGH);
        int val = millis() / 500 % 2;
        if (val == 0)
        {
            if (this->beeps < 3)
            {
                digitalWrite(BUZZER_PIN, HIGH);
                beeps++;
            }
            else
            {
                this->beeps = 0;
                digitalWrite(BUZZER_PIN, LOW);
            }
        }
        else
        {
            digitalWrite(BUZZER_PIN, LOW);
        }
    }
    else
    {
        this->beeping = false;
        digitalWrite(BUZZER_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
    }
}

void Alarm::draw(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(ArialMT_Plain_10);
    if (this->settingStage != SET_MODE)
    {
        display->drawString(64 + x, 7 + y, "Alarm Time");
    }
    else
    {
        display->drawString(64 + x, 7 + y, "Alarm Mode");
    }

    display->setFont(ArialMT_Plain_24);
    if (this->settingStage == SET_HOUR || this->settingStage == SET_MIN)
    {
        display->drawString(64 + x, 17 + y, ":");
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        if (this->settingStage == SET_HOUR)
        {
            display->setColor(WHITE);
            display->fillRect(30 + x, 20 + y, 30, 22);
            display->setColor(BLACK);
        }
        else
        {
            display->setColor(WHITE);
        }
        display->drawString(32 + x, 17 + y, this->settings.getFormatedHours());

        if (this->settingStage == SET_MIN)
        {
            display->setColor(WHITE);
            display->fillRect(68 + x, 20 + y, 30, 22);
            display->setColor(BLACK);
        }
        else
        {
            display->setColor(WHITE);
        }
        display->drawString(70 + x, 17 + y, this->settings.getFormatedMinutes());
    }
    else if (this->settingStage == SET_MODE)
    {
        display->setColor(WHITE);
        display->fillRect(12 + x, 21 + y, 104, 21);
        display->setColor(BLACK);
        display->setFont(ArialMT_Plain_16);
        display->drawString(64 + x, 23 + y, this->settings.getModeString());
    }
    display->setColor(WHITE);
}

void Alarm::stopBeeping()
{
    this->stopBeeps = millis();
    this->beeping = false;
}


void Alarm::beginSetting()
{
    this->stopBeeps = 0;
    this->settingStage = SET_HOUR;
}
void Alarm::endSetting()
{
    this->settingStage = NONE;
}

bool Alarm::nextSettingStage()
{
    if (this->settingStage == SET_MODE)
    {
        this->saveSettings();
        this->endSetting();
        return false;
    }

    if (this->settingStage == SET_HOUR)
    {
        this->settingStage = SET_MIN;
    }
    else if (this->settingStage == SET_MIN)
    {
        this->settingStage = SET_MODE;
    }
    return true;
}

void Alarm::onMinusButtonClick()
{
    if (this->settingStage == SET_HOUR)
    {
        if (this->settings.hours == 0)
        {
            this->settings.hours = 23;
        }
        else
        {
            this->settings.hours -= 1;
        }
    }
    else if (this->settingStage == SET_MIN)
    {
        if (this->getSettings().minutes == 0)
        {
            this->settings.minutes = 60 - DELTA_MIN;
        }
        else
        {
            this->settings.minutes -= DELTA_MIN;
        }
    }
    else if (this->settingStage == SET_MODE)
    {
        if (this->getSettings().mode == OFF)
        {
            this->settings.mode = WEEK_DAYS;
        }
        else
        {
            this->settings.mode = (AlarmMode)(this->settings.mode - 1);
        }
    }
}

void Alarm::onPlusButtonClick()
{
    if (this->settingStage == SET_HOUR)
    {
        if (this->getSettings().hours == 23)
        {
            this->settings.hours = 0;
        }
        else
        {
            this->settings.hours += 1;
        }
    }
    else if (this->settingStage == SET_MIN)
    {
        if (this->getSettings().minutes == 60 - DELTA_MIN)
        {
            this->settings.minutes = 0;
        }
        else
        {
            this->settings.minutes += DELTA_MIN;
        }
    }
    else if (this->settingStage == SET_MODE)
    {
        if (this->getSettings().mode == WEEK_DAYS)
        {
            this->settings.mode = OFF;
        }
        else
        {
            this->settings.mode = (AlarmMode)(this->settings.mode + 1);
        }
    }
}


AlarmSettings Alarm::getSettings()
{
    return this->settings;
}

AlarmSettingStage Alarm::getSettingStage()
{
    return this->settingStage;
}


void Alarm::loadSettings()
{
    Serial.println("Reading Settings...");

    int mode = EEPROM.read(99);
    if (mode > 3)
    {
        // Reset
        this->settings.mode = OFF;
        this->settings.hours = 8;
        this->settings.minutes = 0;
    }
    else
    {
        this->settings.mode = (AlarmMode)mode;
        // Hours
        this->settings.hours = EEPROM.read(100);
        if (this->settings.hours >= 24)
        {
            this->settings.hours = 8;
        }
        // Minutes
        this->settings.minutes = EEPROM.read(101);
        if (this->settings.minutes >= 60)
        {
            this->settings.minutes = 0;
        }
    }

    Serial.print("Alarm mode: ");
    Serial.println(this->settings.mode);
    Serial.print("Alarm time: ");
    Serial.println(this->settings.getFormatedTime());
}

void Alarm::saveSettings()
{
    EEPROM.write(99, this->settings.mode);
    EEPROM.write(100, this->settings.hours);
    EEPROM.write(101, this->settings.minutes);
    EEPROM.commit();
}
