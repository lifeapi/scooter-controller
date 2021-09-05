#include "buzzer.h"

Buzzer::Buzzer(uint8_t pin) {
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

void Buzzer::buzzerProcessing() {
    if (buzzerPaused) {
        if (millis() - buzzerPauseTimer >= buzzerPausePeriod) {
            buzzerPaused = false;
        }
        return;
    }
    if (!buzzerEnabled && buzzerRingCount > 0) {
        buzzerOn();
        return;
    }

    if (buzzerEnabled && (millis() - buzzerTimer >= buzzerPeriod)) {
        buzzerOff();
        return;
    }
}

void Buzzer::on(int count, int buzzerTime) {
    if (buzzerPaused || buzzerEnabled) {
        return;
    }
    buzzerRingCount = count;
    buzzerPeriod = buzzerTime;
    buzzerPausePeriod = buzzerRingCount > 1 ? buzzerTime >> 1 : 0;
}

void Buzzer::buzzerOn() {
    buzzerTimer = millis();
    buzzerEnabled = true;
    digitalWrite(_pin, LOW);
}

void Buzzer::buzzerOff() {
    buzzerEnabled = false;
    if (buzzerRingCount > 0) {
        buzzerRingCount--;
    }
    buzzerPaused = buzzerRingCount > 0;
    if (buzzerPaused) {
        buzzerPauseTimer = millis();
    }
    digitalWrite(_pin, HIGH);
}