#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <EEPROM.h>
#include <MFRC522.h>
#include <SPI.h>
#include <Wire.h>
#include <microDS3231.h>

#include "buzzer.h"

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
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

Buzzer buzzer(BUZZER_PIN);

Adafruit_SSD1306 display(SDA_PIN);

MFRC522 mfrc522(SS_PIN, RST_PIN);

MicroDS3231 rtc;

enum State {
    LOCK,
    CONFIGURATION,
    MANAGEMENT,
    DRIVE
};

uint32_t cardReaderTimer = 0;
uint32_t resetDisplayToStateTimer = 0;

uint32_t resetDisplayToStatePeriod = 0;

unsigned long currentCardUID;
unsigned long userCards[USER_CARD_COUNT];

uint8_t currentUserCardIndex;

int EEPROMstartAddr;
int currentCardIndex;

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
void readUid();
void initCardAdresses();
void increaseCardAdresses();
void displayLock();
void displayUK();
void displayDrive();
void displayManagement();
void displayAdmin(uint8_t currentCardIndex);
void resetDisplayToStateAfterTimeout(uint32_t timeout);
void processDisplayToStateAfterTimeout();
void printTime();
////////////////////////

void setup() {
    Serial.begin(9600);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C(0x3D) (for the  0.96" 128X64 OLED LCD Display)I2C АДРЕС.
    SPI.begin();                                //  инициализация SPI / Init SPI bus.
    mfrc522.PCD_Init();                         // инициализация MFRC522 / Init MFRC522 card.
    Wire.begin();

    if (rtc.lostPower()) {          //  при потере питания
        rtc.setTime(COMPILE_TIME);  // установить время компиляции
    }

    pinMode(LOCK_PIN, OUTPUT);
    pinMode(LED_1_PIN, OUTPUT);
    pinMode(LED_2_PIN, OUTPUT);
    pinMode(LED_3_PIN, OUTPUT);

    pinMode(SDA_PIN, OUTPUT);
    pinMode(SCL_PIN, OUTPUT);

    // Show image buffer on the display hardware.
    // Since the buffer is intialized with an Adafruit splashscreen
    // internally, this will display the splashscreen.
    display.display();
    //  delay(2000);

    // Clear the buffer.
    display.clearDisplay();

    lockScooter();
    EEPROMreadUIDcard();

    Serial.println("Saved cards: ");
    for (int i = 0; i < USER_CARD_COUNT; i++) {
        Serial.println((i + 1) + " -> " + userCards[i]);
    }
    
}


void printTime() {
  Serial.print(rtc.getHours());
  Serial.print(":");
  Serial.print(rtc.getMinutes());
  Serial.print(":");
  Serial.print(rtc.getSeconds());
  Serial.print(" ");
  Serial.print(rtc.getDay());
  Serial.print(" ");
  Serial.print(rtc.getDate());
  Serial.print("/");
  Serial.print(rtc.getMonth());
  Serial.print("/");
  Serial.println(rtc.getYear());
}

void loop() {
    printTime();
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
                cardReaderTimer  = millis();
                displayAdmin(currentCardIndex);
                if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
                    readUid();
                    EEPROMwriteUIDcard();
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
        display.println("   SKIP     RECORD   ");
    } else {
        // "Card UID" / № карты - long, занимает 4 байта - нужно резать на 4 части EEPROM.
        EEPROM.write(EEPROMstartAddr, currentCardUID & 0xFF);
        EEPROM.write(EEPROMstartAddr + 1, (currentCardUID & 0xFF00) >> 8);
        EEPROM.write(EEPROMstartAddr + 2, (currentCardUID & 0xFF0000) >> 16);
        EEPROM.write(EEPROMstartAddr + 3, (currentCardUID & 0xFF000000) >> 24);

        display.println("RECORD OK! IN MEMORY ");
    }

    display.setTextSize(3);
   // display.setCursor(400, 40);
    display.println(currentCardIndex);  // Выводим № записанной ячейки памяти.
    display.display();
    increaseCardAdresses();
    buzzer.on();

    if (currentCardIndex == USER_CARD_COUNT) {  // если записали все карты
        clearDisplay();
        display.setTextSize(3);  // Размер текста (3).
        display.println("RECORD FINISH");
        display.display();
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
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.setCursor(0, 0);
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

void readUid() {
    currentCardUID = 0;
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        currentCardUID = currentCardUID * 256 + mfrc522.uid.uidByte[i];
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
    display.setCursor(20, 20);
    display.setTextSize(4);
    display.print("LOCK");
    display.display();
}

void displayUK() {
    clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.println("Unknown");
    display.println("card");
    display.display();
}

void displayDrive() {
    clearDisplay();
    display.setCursor(20, 20);
    display.setTextSize(3);
    display.println("DRIVE");
    display.setTextSize(2);
    display.print(rtc.getHours() + ":" + rtc.getMinutes());
    display.display();
}

void displayManagement() {
    clearDisplay();
    display.setTextSize(2);
    display.println(rtc.getHours() + ":" + rtc.getMinutes());
    display.println("LOCK");
    display.println("DAY LIGTH");
    display.println("HIGTH LIGTH");
    display.println("LED");
    display.display();
}

void displayAdmin(uint8_t currentCardIndex) {
    clearDisplay();
    display.setTextSize(2);
    display.println("ADMIN MODE");
    display.setTextSize(1);
    display.println("Add user card");
    display.println(currentCardIndex + " / " + USER_CARD_COUNT);
    display.display();
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
