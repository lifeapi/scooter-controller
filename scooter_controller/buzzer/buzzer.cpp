#include <buzzer.h>

Buzzer::Buzzer(uint8_t pin)
{
    _pin = pin;

    buzzerRingCount = 0;
    buzzerTimer = 0;
    buzzerPauseTimer = 0;
    buzzerPeriod = 0;
    buzzerPausePeriod = 0;
    buzzerEnabled = false;
    buzzerPaused = false;

    pinMode(_pin, OUTPUT);
}

void buzzerProcessing()
{
    if (buzzerPaused)
    {
        if (millis() - buzzerPauseTimer >= buzzerPausePeriod)
        {
            buzzerPaused = false;
        }
    }
    else
    {
        if (!buzzerEnabled && buzzerTimer > 0)
        {
            buzzerOn();
        }
    }
    if (buzzerEnabled && (millis() - buzzerTimer >= buzzerPeriod))
    {
        buzzerOff();
    }
}

void on(int count)
{
    on(count, 400)
}

void on(int count, int buzzerTime)
{
    buzzerRingCount = count;
    buzzerPeriod = buzzerTime;
    buzzerTimer = millis() + buzzerPeriod;
    buzzerPausePeriod = buzzerRingCount > 1 ? buzzerTime >> 1 : 0;
}

void buzzerOn()
{
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerEnabled = true;
    buzzerPaused = false;
}

void buzzerOff()
{
    digitalWrite(BUZZER_PIN, LOW);
    buzzerEnabled = false;
    buzzerRingCount--;
    buzzerPaused = buzzerRingCount > 0;
    if (buzzerPaused)
    {
        buzzerPauseTimer = millis() + buzzerPausePeriod;
    }