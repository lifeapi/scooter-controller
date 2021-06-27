#pragma once
#include <Arduino.h>

class Buzzer
{
public:
    Buzzer(uint8_t pin);
    void buzzerProcessing();
    void on(int count = 1, int buzzerTime = 400);

private:
    uint8_t _pin;
    uint8_t buzzerRingCount;
    uint32_t buzzerTimer;
    uint32_t buzzerPauseTimer;
    uint32_t buzzerPeriod;
    uint32_t buzzerPausePeriod;
    boolean buzzerEnabled;
    boolean buzzerPaused;

    void buzzerOn();
    void buzzerOff();
};