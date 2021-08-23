//#include <Adafruit_GFX.h>
//#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <EEPROM.h>
#include <MFRC522.h>
#include <RTClib.h>
//#include <SPI.h>
//#include <Wire.h>
//#include <microWire.h>

#include "buzzer.h"

//#define USE_MICRO_WIRE
#define OLED_SPI_SPEED 4000000ul
#include <GyverOLED.h>

#define SCK_PIN 13   // not use in sketch
#define MISO_PIN 12  // not use in sketch
#define MOSI_PIN 11  // not use in sketch

#define SS_PIN 10
#define RST_PIN 9

#define LOCK_PIN 8
#define LED_1_PIN 7
#define LED_2_PIN 6
#define LED_3_PIN 5

#define BTN_UP_PIN 4
#define BTN_OK_PIN 3
#define BTN_DOWN_PIN 2

#define BUZZER_PIN A0

#define SDA_PIN A4
#define SCL_PIN A5

#define ADMIN_CARD_UID 2427348274
#define USER_CARD_COUNT 5
#define MENU_ITEM_COUNT 4

#define CARD_READER_PERIOD 500
#define UPDATE_TIME_PERIOD 1000
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

Buzzer buzzer(BUZZER_PIN);

//Adafruit_SSD1306 display(SDA_PIN);

GyverOLED<SSD1306_128x64> oled;

MFRC522 mfrc522(SS_PIN, RST_PIN);

RTC_DS3231 rtc;

enum State {
    LOCK,
    CONFIGURATION,
    MANAGEMENT,
    DRIVE
};

const char daysOfTheWeek[7][12] = {"Воскресенье", "Понедельник", "Вторник", "Среда", "Четверг", "Пятница", "Суббота"};

String temp;
String time;
String fullTime;
String weekDay;

uint32_t driveModeTimer = 0;
uint32_t updateTimeTimer = 0;
uint32_t cardReaderTimer = 0;
uint32_t resetDisplayToStateTimer = 0;

uint32_t resetDisplayToStatePeriod = 0;

unsigned long currentCardUID = 0;
unsigned long userCards[USER_CARD_COUNT];

uint8_t currentUserCardIndex;

int EEPROMstartAddr;
int currentCardIndex = 0;

const int eepromStartAddrUID = 0;
const int eepromStartAddrConfig = USER_CARD_COUNT * 5;

State currentState;

uint8_t configuration[MENU_ITEM_COUNT];

// functions declaration
void EEPROMwriteUIDcard();
void EEPROMreadUIDcard();
void EEPROMreadConfiguration();
void clearDisplay();
void unlockScooter();
void unknownCard();
void lockScooter();
boolean readUid();
void initCardAdresses();
void increaseCardAdresses();
void displayLock();
void displayUK();
void displayDrive();
void displayManagement();
void displayAdmin(uint8_t currentCardIndex);
void resetDisplayToStateAfterTimeout(uint32_t timeout);
void processDisplayToStateAfterTimeout();
void updateTime();
////////////////////////

void setup() {
    Serial.begin(9600);
    SPI.begin();
    mfrc522.PCD_Init();
    Wire.begin();
    oled.init();

    Serial.print("Compiled time: ");
    Serial.print(__DATE__);
    Serial.print(" ");
    Serial.println(__TIME__);

    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        Serial.flush();
        abort();
    }

    if (rtc.lostPower()) {
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    pinMode(LOCK_PIN, OUTPUT);
    pinMode(LED_1_PIN, OUTPUT);
    pinMode(LED_2_PIN, OUTPUT);
    pinMode(LED_3_PIN, OUTPUT);
    pinMode(SDA_PIN, OUTPUT);
    pinMode(SCL_PIN, OUTPUT);

    clearDisplay();

    lockScooter();
    EEPROMreadUIDcard();

    Serial.println("Saved cards: ");
    for (int i = 0; i < USER_CARD_COUNT; i++) {
        Serial.print(i + 1, DEC);
        Serial.print(" -> ");
        Serial.println(userCards[i]);
    }
}

void updateTime() {
    if (millis() - updateTimeTimer >= UPDATE_TIME_PERIOD) {
        updateTimeTimer = millis();
        DateTime now = rtc.now();
        time = String(now.hour()) + ":" + String(now.minute());
        fullTime = String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second()) + " " + String(now.day()) + "." + String(now.month()) + "." + String(now.year());
        weekDay = daysOfTheWeek[now.dayOfTheWeek()];
        temp = String(rtc.getTemperature(), 2);
    }
}

void loop() {
    updateTime();
    buzzer.buzzerProcessing();
    processDisplayToStateAfterTimeout();
    switch (currentState) {
        case LOCK:
            if (millis() - cardReaderTimer >= CARD_READER_PERIOD) {
                cardReaderTimer = millis();
                if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
                    readUid();
                    if (currentCardUID == ADMIN_CARD_UID) {
                        currentState = CONFIGURATION;
                        initCardAdresses();
                        buzzer.on(3, 500);
                    } else {
                        for (int i = 0; i < USER_CARD_COUNT; i++) {
                            if (userCards[i] == currentCardUID) {
                                unlockScooter();
                                return;
                            }
                        }
                        unknownCard();
                    }
                }
            }
            break;
        case CONFIGURATION: {
            if (millis() - cardReaderTimer >= CARD_READER_PERIOD) {
                cardReaderTimer = millis();
                displayAdmin(currentCardIndex);
                if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
                    if (readUid()) {
                        EEPROMwriteUIDcard();
                    }
                }
            }
            break;
        }
        case MANAGEMENT: {
            displayManagement();
            break;
        }
        case DRIVE: {
            displayDrive();
            break;
        }
    }
}

void EEPROMwriteUIDcard() {
    clearDisplay();
    if (currentCardUID == ADMIN_CARD_UID) {
        oled.println("Карта №");
        oled.print(currentCardIndex + 1, DEC);
        oled.println(" пропущенна");
    } else {
        // "Card UID" / № карты - long, занимает 4 байта - нужно резать на 4 части EEPROM.
        EEPROM.write(EEPROMstartAddr, currentCardUID & 0xFF);
        EEPROM.write(EEPROMstartAddr + 1, (currentCardUID & 0xFF00) >> 8);
        EEPROM.write(EEPROMstartAddr + 2, (currentCardUID & 0xFF0000) >> 16);
        EEPROM.write(EEPROMstartAddr + 3, (currentCardUID & 0xFF000000) >> 24);

        oled.print("Карта №");

        oled.print(currentCardIndex + 1, DEC);
        oled.println(" записанна");
    }

    oled.update();
    increaseCardAdresses();
    buzzer.on();

    if (currentCardIndex == USER_CARD_COUNT) {  // если записали все карты
        delay(1000);
        oled.println("Запись окончена");

        oled.update();
        EEPROMreadUIDcard();  // Запускаем функцию, для перезаписи массива userCards, данными из EEPROM.
        lockScooter();
    }
}

void EEPROMreadUIDcard() {
    for (int i = 0; i < USER_CARD_COUNT; i++) {
        int eepromAddr = i * 5;
        byte val = EEPROM.read(eepromAddr + 3);
        userCards[i] = (userCards[i] << 8) | val;
        val = EEPROM.read(eepromAddr + 2);
        userCards[i] = (userCards[i] << 8) | val;
        val = EEPROM.read(eepromAddr + 1);
        userCards[i] = (userCards[i] << 8) | val;
        val = EEPROM.read(eepromAddr);
        userCards[i] = (userCards[i] << 8) | val;
    }
}

void EEPROMreadConfiguration() {
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        int eepromAddr = i * 5;
        byte val = EEPROM.read(eepromAddr + 3);
        userCards[i] = (userCards[i] << 8) | val;
        val = EEPROM.read(eepromAddr + 2);
        userCards[i] = (userCards[i] << 8) | val;
        val = EEPROM.read(eepromAddr + 1);
        userCards[i] = (userCards[i] << 8) | val;
        val = EEPROM.read(eepromAddr);
        userCards[i] = (userCards[i] << 8) | val;
    }
}

void clearDisplay() {
    oled.home();
    oled.clear();
    oled.update();
}

void unlockScooter() {
    digitalWrite(LOCK_PIN, HIGH);
    currentState = DRIVE;
    buzzer.on();
}

void unknownCard() {
    resetDisplayToStateAfterTimeout(1000);
    buzzer.on(1, 2000);
    displayUK();
}

void lockScooter() {
    digitalWrite(LOCK_PIN, LOW);
    currentState = LOCK;
    displayLock();
    buzzer.on();
}

boolean readUid() {
    unsigned long tempCurrentCardUID = 0;
    currentCardUID = 0;
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        tempCurrentCardUID = tempCurrentCardUID * 256 + mfrc522.uid.uidByte[i];
    }
    if (tempCurrentCardUID == currentCardUID) {
        return false;
    } else {
        currentCardUID = tempCurrentCardUID;
        return true;
    }
}

void initCardAdresses() {
    EEPROMstartAddr = 0;
    currentCardIndex = 0;
}

void increaseCardAdresses() {
    EEPROMstartAddr += 5;
    currentCardIndex = EEPROMstartAddr / 5;
}

void displayLock() {
    clearDisplay();
    oled.setScale(3);
    oled.print("Заблокированно");
    oled.update();
}

void displayUK() {
    clearDisplay();
    oled.setScale(2);
    oled.setCursor(0, 20);
    oled.println("Unknown");
    oled.println("card");
    oled.update();
}

void displayDrive() {
    if (millis() - driveModeTimer >= UPDATE_TIME_PERIOD) {
        Serial.println(" -> " + time);
        driveModeTimer = millis();
        clearDisplay();
        oled.setCursor(20, 20);
        oled.setScale(3);
        oled.println(time);
        oled.setScale(2);
        oled.print("Поездка");
        oled.update();
    }
}

void displayManagement() {
    clearDisplay();
    oled.setScale(1);
    oled.println(time);
    oled.setScale(2);
    oled.println("Блокировка");
    oled.println("ДХО");
    oled.println("Свет");
    oled.println("Подсветка");
    oled.update();
}

void displayAdmin(uint8_t currentCardIndex) {
    clearDisplay();
    oled.setScale(1);
    oled.println("Режим администратора");
    oled.fastLineH(0, 16, 16);
    oled.setScale(1);
    oled.println("Запись карты");
    oled.print(currentCardIndex + 1, DEC);
    oled.print(" / ");
    oled.print(USER_CARD_COUNT, DEC);
    oled.update();
}

void resetDisplayToStateAfterTimeout(uint32_t timeout) {
    resetDisplayToStatePeriod = timeout;
    resetDisplayToStateTimer = millis();
}

void processDisplayToStateAfterTimeout() {
    if (resetDisplayToStateTimer != 0 && millis() - resetDisplayToStateTimer >= resetDisplayToStatePeriod) {
        resetDisplayToStateTimer = 0;
        switch (currentState) {
            case LOCK:
                displayLock();
                break;
            case CONFIGURATION:
                displayAdmin(currentCardIndex);
                break;
            case MANAGEMENT:
                displayManagement();
                break;
            case DRIVE:
                displayDrive();
                break;
        }
    }
}
