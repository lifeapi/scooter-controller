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
    } else {
        if (!buzzerEnabled && buzzerTimer > 0) {
            buzzerOn();
        }
    }
    if (buzzerEnabled && (millis() - buzzerTimer >= buzzerPeriod)) {
        buzzerOff();
    }
}

void Buzzer::on(int count, int buzzerTime) {
    buzzerRingCount = count;
    buzzerPeriod = buzzerTime;
    buzzerTimer = millis();
    buzzerPausePeriod = buzzerRingCount > 1 ? buzzerTime >> 1 : 0;
}

void Buzzer::buzzerOn() {
    digitalWrite(_pin, LOW);
    buzzerEnabled = true;
    buzzerPaused = false;
}

void Buzzer::buzzerOff() {
    digitalWrite(_pin, HIGH);
    buzzerEnabled = false;
    buzzerRingCount--;
    buzzerPaused = buzzerRingCount > 0;
    if (buzzerPaused) {
        buzzerPauseTimer = millis() + buzzerPausePeriod;
    }
}