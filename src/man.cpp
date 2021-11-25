#include <Arduino.h>
#include <EEPROM.h>
#include <MFRC522.h>

#include "GyverButton.h"
#include "GyverOLED.h"
#include "RTClib.h"
#include "buzzer.h"
#include "constants.h"

GButton btnUp(BTN_UP_PIN);
GButton btnOk(BTN_OK_PIN);
GButton btnDown(BTN_DOWN_PIN);
Buzzer buzzer(BUZZER_PIN);
GyverOLED<SSD1306_128x64> oled;
MFRC522 mfrc522(SS_PIN, RST_PIN);
RTC_DS3231 rtc;

enum State {
    LOCK,
    CONFIGURATION,
    MANAGEMENT,
    DRIVE
};

int8_t selectedMenuItem = 0;
int8_t currentMenuItem = 0;

String temp;
String time;
String fullTime;
String weekDay;
String firmwareDate;

uint32_t driveModeTimer = 0;
uint32_t updateTimeTimer = 0;
uint32_t cardReaderTimer = 0;
uint32_t resetDisplayToStateTimer = 0;

uint32_t resetDisplayToStatePeriod = 0;

int8_t startNavigation = 0;
int8_t endNavigation = countMenuItemOnDisplay - 1;

unsigned long currentCardUID = 0;
unsigned long userCards[USER_CARD_COUNT];

uint8_t currentUserCardIndex;

int EEPROMstartAddr;
int currentCardIndex = 0;

State currentState;

uint8_t configuration[COUNT_MENU_ITEM];

bool updateDisplay = true;

// functions declaration
void EEPROMwriteUIDcard();
void EEPROMreadUIDcard();
void EEPROMreadConfiguration();
void writeConfigurationValue(uint8_t menuItem);
void applyConfiguration();
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

void management();
void navigaionMode();
void selectionMode();
void callAction();

void callExit();
void callLock();
void processBooleanMenuItem(uint8_t index);
void callInformation();

void initNavigation();
void displayManagement();
void displayAdmin(uint8_t currentCardIndex);
void resetDisplayToStateAfterTimeout(uint32_t timeout = 1000);
void processDisplayToStateAfterTimeout();
void updateTime();
void needUpdateDisplay();
String getMiniteWord(uint32_t n);
int getLastDigit(uint32_t n);
////////////////////////

void setup() {
    Serial.begin(9600);
    SPI.begin();
    mfrc522.PCD_Init();
    Wire.begin();
    oled.init();

    Serial.print(F("Compiled time: "));
    Serial.print(__DATE__);
    Serial.print(F(" "));
    Serial.println(__TIME__);

    DateTime fd = DateTime(F(__DATE__), F(__TIME__));
    String d = String(fd.day());
    String m = String(fd.month());
    String y = String(fd.year());
    String h = String(fd.hour());
    String mm = String(fd.minute());
    String s = String(fd.second());
    firmwareDate = d + "." + m + "." + y + " " + h + ":" + mm + ":" + s;

    if (!rtc.begin()) {
        Serial.println(F("Couldn't find RTC"));
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

    // инициализация пищалки
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(BUZZER_PIN, HIGH);

    btnUp.setType(LOW_PULL);
    btnOk.setType(LOW_PULL);
    btnDown.setType(LOW_PULL);

    // инициализация всех пинов нагрузки
    digitalWrite(LOCK_PIN, HIGH);

    clearDisplay();
    lockScooter();
    EEPROMreadUIDcard();
    EEPROMreadConfiguration();
    applyConfiguration();

    Serial.println(F("Saved cards: "));
    for (int i = 0; i < USER_CARD_COUNT; i++) {
        Serial.print(i + 1, DEC);
        Serial.print(F(" -> "));
        Serial.println(userCards[i]);
    }

    Serial.println(F("Saved configuration: "));
    for (int i = 0; i < COUNT_MENU_ITEM; i++) {
        Serial.print(i, DEC);
        Serial.print(F(" -> "));
        Serial.println(configuration[i]);
    }
}

void updateTime() {
    if (millis() - updateTimeTimer >= UPDATE_TIME_PERIOD) {
        updateTimeTimer = millis();
        DateTime now = rtc.now();
        String hourS = now.hour() < 10 ? "0" + String(now.hour()) : String(now.hour());
        String minuteS = now.minute() < 10 ? "0" + String(now.minute()) : String(now.minute());

        time = hourS + ":" + minuteS;
        fullTime = time + ":" + String(now.second()) + " " + String(now.day()) + "." + String(now.month()) + "." + String(now.year());

        char buffer[11];
        strcpy_P(buffer, (char*)pgm_read_word(&(daysOfTheWeek[now.dayOfTheWeek()])));
        weekDay = String(buffer);

        temp = String(rtc.getTemperature(), 2);
    }
}

void loop() {
    btnUp.tick();
    btnOk.tick();
    btnDown.tick();
    buzzer.buzzerProcessing();
    updateTime();
    processDisplayToStateAfterTimeout();
    switch (currentState) {
        case LOCK:
            displayLock();
            if (millis() - cardReaderTimer >= CARD_READER_PERIOD) {
                cardReaderTimer = millis();
                if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
                    needUpdateDisplay();
                    readUid();
                    if (currentCardUID == ADMIN_CARD_UID) {
                        currentState = CONFIGURATION;
                        initCardAdresses();
                        buzzer.on(3, 200);
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
            displayAdmin(currentCardIndex);
            if (millis() - cardReaderTimer >= CARD_READER_PERIOD) {
                cardReaderTimer = millis();
                if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
                    if (readUid()) {
                        EEPROMwriteUIDcard();
                    }
                }
            }
            break;
        }
        case MANAGEMENT: {
            management();
            break;
        }
        case DRIVE: {
            if (btnUp.isHold() && btnDown.isHold()) {
                needUpdateDisplay();
                initNavigation();
                currentState = MANAGEMENT;
                buzzer.on();
                clearDisplay();
            } else {
                displayDrive();
            }
            break;
        }
    }
}

void EEPROMwriteUIDcard() {
    clearDisplay();
    if (currentCardUID == ADMIN_CARD_UID) {
        oled.print(F("Карта #"));
        oled.println(currentCardIndex + 1, DEC);
        oled.println(F("пропущена"));
    } else {
        // "Card UID" / № карты - long, занимает 4 байта - нужно резать на 4 части EEPROM.
        EEPROM.write(EEPROMstartAddr, currentCardUID & 0xFF);
        EEPROM.write(EEPROMstartAddr + 1, (currentCardUID & 0xFF00) >> 8);
        EEPROM.write(EEPROMstartAddr + 2, (currentCardUID & 0xFF0000) >> 16);
        EEPROM.write(EEPROMstartAddr + 3, (currentCardUID & 0xFF000000) >> 24);

        oled.print(F("Карта #"));
        oled.println(currentCardIndex + 1, DEC);
        oled.println(F("записана"));
    }

    increaseCardAdresses();
    buzzer.on();

    if (currentCardIndex == USER_CARD_COUNT) {  // если записали все карты
        oled.invertText(true);
        oled.println(F(" Запись"));
        oled.println(F("окончена"));
        oled.invertText(false);
        EEPROMreadUIDcard();  // Запускаем функцию, для перезаписи массива userCards, данными из EEPROM.
        resetDisplayToStateAfterTimeout(2000);
        lockScooter();
    }
    oled.update();
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

void EEPROMreadConfiguration() {  // чтение конфигурации
    for (int i = 0; i < COUNT_MENU_ITEM; i++) {
        int eepromAddr = i * 2 + USER_CARD_COUNT * 5;
        byte val = EEPROM.read(eepromAddr);
        configuration[i] = (configuration[i] << 8) | val;
    }
}

void writeConfigurationValue(uint8_t menuItem) {
    int eepromAddr = menuItem * 2 + USER_CARD_COUNT * 5;
    EEPROM.write(eepromAddr, configuration[menuItem]);
    applyConfiguration();
}

void applyConfiguration() {
    digitalWrite(LED_1_PIN, configuration[2]);
    digitalWrite(LED_2_PIN, configuration[3]);
    digitalWrite(LED_3_PIN, configuration[4]);
}

void clearDisplay() {
    oled.home();
    oled.clear();
    oled.update();
}

void unlockScooter() {
    digitalWrite(LOCK_PIN, LOW);
    currentState = DRIVE;
    driveModeTimer = 0;
    buzzer.on();
    clearDisplay();
}

void unknownCard() {
    displayUK();
    resetDisplayToStateAfterTimeout(2000);
    buzzer.on(1, 2000);
}

void lockScooter() {
    digitalWrite(LOCK_PIN, HIGH);
    currentState = LOCK;
    buzzer.on(2, 300);
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
    if (updateDisplay) {
        updateDisplay = false;
    } else {
        return;
    }
    clearDisplay();
    oled.drawBitmap(0, 0, locked, 128, 64, 1);
    oled.update();
}

void displayUK() {
    if (updateDisplay) {
        updateDisplay = false;
    } else {
        return;
    }
    clearDisplay();
    oled.drawBitmap(0, 0, skill, 128, 64, 1);
    oled.update();
}

void displayDrive() {
    if (millis() - driveModeTimer >= UPDATE_TIME_PERIOD) {
        driveModeTimer = millis();
        oled.home();
        oled.textMode(BUF_REPLACE);
        oled.setScale(2);
        oled.setCursor(30, 0);
        oled.println(time);
        oled.println();
        oled.println(F("В пути:"));
        uint32_t timeInRide = driveModeTimer / 60000;
        oled.print(timeInRide, DEC);
        oled.print(" " + getMiniteWord(timeInRide));
        oled.update();
    }
}

String getMiniteWord(uint32_t n) {
    if (n > 10 && n < 20) {
        return "минут";
    }
    int lastDigit = n < 10 ? n : getLastDigit(n);
    switch (lastDigit) {
        case 1:
            return "минута";
        case 2:
        case 3:
        case 4:
            return "минуты";
        default:
            return "минут";
    }
}

int getLastDigit(uint32_t n) {
    return n % 10;
}

void management() {
    if (selectedMenuItem == -1) {
        displayManagement();
        navigaionMode();
    } else {
        selectionMode();
    }
}

void initNavigation() {
    startNavigation = 0;
    endNavigation = countMenuItemOnDisplay - 1;
}
void navigaionMode() {
    if (btnDown.isClick()) {
        needUpdateDisplay();
        if (currentMenuItem == COUNT_MENU_ITEM - 1) {
            currentMenuItem = 0;
        } else {
            currentMenuItem++;
        }
    }

    if (btnOk.isClick()) {
        needUpdateDisplay();
        selectedMenuItem = currentMenuItem;
    }

    if (btnUp.isClick()) {
        needUpdateDisplay();
        if (currentMenuItem == 0) {
            currentMenuItem = COUNT_MENU_ITEM - 1;
        } else {
            currentMenuItem--;
        }
    }
}
void selectionMode() {
    callAction();
}

void callAction() {
    switch (currentMenuItem) {
        case 0: {  // Выход
            callExit();
        } break;
        case 1: {  //Блокировка
            callLock();
            break;
        }
        case 2: {  // ДХО
            processBooleanMenuItem(currentMenuItem);
            break;
        }
        case 3: {  // Свет
            processBooleanMenuItem(currentMenuItem);
            break;
        }
        case 4: {  // Подсветка
            processBooleanMenuItem(currentMenuItem);
            break;
        }
        case 5: {  // Информация
            callInformation();
            break;
        }
    }
}

void displayManagement() {
    if (updateDisplay) {
        updateDisplay = false;
    } else {
        return;
    }

    oled.home();
    oled.textMode(BUF_REPLACE);
    oled.setScale(2);

    if (currentMenuItem > endNavigation) {
        startNavigation = currentMenuItem - countMenuItemOnDisplay + 1;
        endNavigation = currentMenuItem;
    } else if (currentMenuItem < startNavigation) {
        startNavigation = currentMenuItem;
        endNavigation = currentMenuItem + countMenuItemOnDisplay - 1;
    }

    for (int8_t i = startNavigation; i <= endNavigation; i++) {
        oled.invertText(selectedMenuItem == i);
        if (currentMenuItem == i) {
            oled.print(F(">"));
        } else {
            oled.print(F(" "));
        }

        char buffer[11];
        oled.print(strcpy_P(buffer, (char*)pgm_read_word(&(menu[i]))));

        oled.println(F("        "));  // необходимо для перетирания текста
    }
    oled.invertText(false);
    oled.update();
}

void callExit() {
    currentState = DRIVE;
    clearDisplay();
    needUpdateDisplay();
    selectedMenuItem = -1;
}

void callLock() {
    lockScooter();
    needUpdateDisplay();
    selectedMenuItem = -1;
}

void processBooleanMenuItem(uint8_t index) {
    if (updateDisplay) {
        updateDisplay = false;
    } else {
        if (btnOk.isClick()) {
            needUpdateDisplay();
            selectedMenuItem = -1;
            writeConfigurationValue(index);
            buzzer.on();
        } else if (btnUp.isClick() || btnDown.isClick()) {
            needUpdateDisplay();
            configuration[index] = configuration[index] == 0 ? 1 : 0;
            applyConfiguration();
        }
    }
}

void callInformation() {
    if (updateDisplay) {
        updateDisplay = false;
    } else {
        if (btnOk.isClick()) {
            needUpdateDisplay();
            selectedMenuItem = -1;
        }

        return;
    }
    clearDisplay();
    oled.setScale(1);
    oled.print(F("Время "));
    oled.println(fullTime);  // не выводиться
    oled.println(weekDay);
    oled.print(F("Темп "));
    oled.println(temp);
    oled.print(F("Прошивка "));
    oled.println(firmwareDate);
    oled.update();
}

void displayAdmin(uint8_t currentCardIndex) {
    if (updateDisplay) {
        updateDisplay = false;
    } else {
        return;
    }
    clearDisplay();
    oled.setScale(2);
    oled.println(F("Админка"));
    oled.println();
    oled.println(F("Запись"));
    oled.print(F("карты "));
    oled.print(currentCardIndex + 1, DEC);
    oled.print("/");
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
        needUpdateDisplay();
    }
}

void needUpdateDisplay() {
    updateDisplay = true;
}
