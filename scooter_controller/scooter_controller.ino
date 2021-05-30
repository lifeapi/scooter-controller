#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MFRC522.h>

#include <buzzer.h>

#define SCK_PIN 13 // not use in sketch
#define MISO_PIN 12 // not use in sketch
#define MOSI_PIN 11 // not use in sketch

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

Buzzer buzzer(BUZZER_PIN);

Adafruit_SSD1306 display(SDA_PIN);

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Объект MFRC522C / reate MFRC522 instance.

enum State {
  LOCK,
  CONFIGURATION,
  DRIVE
};

const unsigned long adminCardUID = 4198836864;
const uint8_t userCardCount = 5;

unsigned long currentCardUID;
unsigned long userCards[userCardCount];
uint8_t currentUserCardIndex;

int EEPROMstartAddr;

State currentState;


void setup() {
  Serial.begin(9600);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // initialize with the I2C addr 0x3C (for the  0.96" 128X64 OLED LCD Display)I2C АДРЕС.
  SPI.begin(); //  инициализация SPI / Init SPI bus.
  mfrc522.PCD_Init(); // инициализация MFRC522 / Init MFRC522 card.

  pinMode(LOCK_PIN, OUTPUT);
  pinMode(LED_1_PIN, OUTPUT);
  pinMode(LED_2_PIN, OUTPUT);
  pinMode(LED_3_PIN, OUTPUT);

  pinMode(SDA_PIN, OUTPUT);
  pinMode(SCL_PIN, OUTPUT);

  lockScooter();
  EEPROMreadUIDcard();

  Serial.println("Все сохранённые карты: ");
  for (int i = 0; i < userCardCount; i++) {
    Serial.println((i + 1) + " -> " + userCards[i]);
  }
}

void loop() {
 buzzer.buzzerProcessing();
   switch (currentState) {
    case LOCK:
      if ( mfrc522.PICC_IsNewCardPresent() &&  mfrc522.PICC_ReadCardSerial()) {
        readUid();
        if (currentCardUID == adminCardUID) {
          currentState = CONFIGURATION;
          EEPROMstartAddr = 0;
          clearDisplay();
          display.println("   START    RECORD   Card UID   CLIENT");
          buzzer.on(3, 500);
        } else {
          for (currentUserCardIndex = 0; currentUserCardIndex < userCardCount; currentUserCardIndex++) {
            if (userCards[currentUserCardIndex] == currentCardUID) {
              unlockScooter();
              return;
            }
          }
          unknownCard();
        }
      }
      break;
    case CONFIGURATION: {
        if ( mfrc522.PICC_IsNewCardPresent() &&  mfrc522.PICC_ReadCardSerial()) {
          readUid();
          EEPROMwriteUIDcard();
        }
        break;
      case DRIVE: {

          break;
        }
      }
  }

  void EEPROMwriteUIDcard() {
    if (currentCardUID == adminCardUID) {
      clearDisplay();
      display.println("   SKIP     RECORD   ");
      display.setTextSize(3);
      display.setCursor(40, 40);
      display.println(EEPROMstartAddr / 5); // Выводим № пропущенной ячейки памяти.
      display.display();
      EEPROMstartAddr += 5; // Пропускаем запись в ячейку памяти, если не хотим записывать туда "Card UID".
      buzzer.on(1);
    } else {
      // "Card UID" / № карты - long, занимает 4 байта - нужно резать на 4 части EEPROM.
      EEPROM.write(EEPROMstartAddr, currentCardUID & 0xFF);
      EEPROM.write(EEPROMstartAddr + 1, (currentCardUID & 0xFF00) >> 8);
      EEPROM.write(EEPROMstartAddr + 2, (currentCardUID & 0xFF0000) >> 16);
      EEPROM.write(EEPROMstartAddr + 3, (currentCardUID & 0xFF000000) >> 24);

      display.println("RECORD OK! IN MEMORY ");
      display.setTextSize(3);
      display.setCursor(50, 40);
      display.println(EEPROMstartAddr / 5); // Выводим № записанной ячейки памяти.
      display.display();
      EEPROMstartAddr += 5; // Прибавляем 5 к стартовой ячейки записи.
      buzzer.on(1);
    }
  }

  if (EEPROMstartAddr / 5 == userCardCount)  { // если записали все карты
    clearDisplay();
    display.setTextSize(3); // Размер текста (3).
    display.println("RECORD FINISH");
    display.display();
    buzzer.on(1);
    EEPROMreadUIDcard(); // Запускаем функцию, для перезаписи массива userCards, данными из EEPROM.
  }
  currentCardUID = 0;
}

void EEPROMreadUIDcard() {
  for (int i = 0; i < userCardCount; i++) {
    int eepromAddr = i * 5;
    byte val = EEPROM.read(eepromAddr + 3);
    userCards[i] = (userCards[i] << 8) | val;
    val = EEPROM.read(eepromAddr + 2);
    userCards[currentUserCardIndex] = (userCards[i] << 8) | val;
    val = EEPROM.read(eepromAddr + 1);
    userCards[currentUserCardIndex] = (userCards[i] << 8) | val;
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

  clearDisplay();
  display.print("Разблокирован");
  display.setTextSize(3);
  display.display();

  buzzer.on(1)
}

void unknownCard() {
  clearDisplay();
  display.print("Неизвестная карта");
  display.setTextSize(3);
  display.display();
  buzzer.on(1, 2000)
}

void lockScooter() {
  digitalWrite(LOCK_PIN, LOW);
  currentState = LOCK;

  clearDisplay();
  display.print("Заблокирован");
  display.setTextSize(3);
  display.display();

  buzzer.on(1, 1000)
}

void readUid() {
  currentCardUID = 0;
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    currentCardUID = currentCardUID * 256 + mfrc522.uid.uidByte[i];
  }
}
}
