/* Дмитрий Осипов. http://www.youtube.com/user/d36073?feature=watch

  v.01 Arduino NFC EEPROM электронный ключ RC522 Card Read Module RFID OLED LCD Display
  Version 0.1 2014/09/01
  ====================================
  Каждый электронный NFC (билет / карта / проездной / брелок), имеет свой уникальный номер "UID",
  на основе этой уникальности, организуем систему управления, например контроль доступа.

  Инструкция:
  Сначала записываем "вручную" в sketch № "Card UID" карты администратора,
  для запуска режима записи в EEPROM (энергонезависимую память)"Card UID" пользователей, максимум 50 карт.
  В "режиме управления", поднеся к "Card Read" карту администратора, запускаем "режим записи" "Card UID" пользователей в 50 ячеек
  памяти, если нужно пропустить какую-нибудь ячейку, в "режиме записи" поднеся карту администратора, перескочим к следующей ячейке.
  Для выхода из режима записи, надо пройти от 0 до 49 ячейки, или сделать перезагрузку.
  Все записанные "Card UID", сохраняются даже после отключения питания.

  В "режиме управления" подносим к "Card Read" записанную в память / EEPROM карту пользователя,
  в зависимости от "Card UID" поднесённой карты, выполняем любые действия которые сами пропишем в sketch.

  Параллельно выводим на дисплей информацию о всех действиях, и озвучиваем через Piezo / динамик.
  Иногда, могут возникать проблемы у библиотеки Serial, но она итак в коде не используется.
 *********************

  Что нам понадобится:

  1) Arduino.

  2) RC522 Card Read Module
  Mifare RC522 Card Read Module Tags SPI Interface Read and Write.
  http://www.ebay.com/itm/400447291624?ssPageName=STRK:MEWNX:IT&_trksid=p3984.m1439.l2649
  — Поддерживаемые типы карт: MIFARE S50, MIFARE S70,
  MIFARE UltraLight, MIFARE Pro, MIFARE DESfire.
  ! Напряжение / питание: 3.3 v.


  3) Дисплей OLED LCD Display I2C 0.96 IIC Serial 128X64
  http://www.ebay.com/itm/251550002026?ssPageName=STRK:MEWNX:IT&_trksid=p3984.m1439.l2649

  4) Piezo / Динамик.

 **********************************

  Скачать sketch.
  v.01 Arduino NFC EEPROM электронный ключ RC522 Card Read Module RFID OLED LCD Display

  ----------------------------
  Подробную видео инструкцию выложу здесь.
  v.01 Arduino NFC EEPROM электронный ключ RC522 Card Read Module RFID OLED LCD Display

  ============================================


 ***************************************************************
  Вспомогательные видео инструкции, sketch, программы, библиотеки.
 ***************************************************************


  v.02 Arduino NFC билет Метро электронный ключ RC522 Card Read Module RFID
  Скачать sketch.  https://yadi.sk/d/zJpYsmLcTkaYV

  видео v.02 Arduino NFC билет Метро электронный ключ RC522 Card Read Module RFID
  http://www.youtube.com/watch?v=TF8oMHsSfWU

  ----------------------------

  v.01 Arduino Метро Единый билет RC522 Card Read Module RFID.txt
  Скачать sketch. https://yadi.sk/d/L1YHkmcuSjdwy

  видео: Arduino Метро Единый билет RC522 Card Read Module RFID NFC
  http://www.youtube.com/watch?v=z6-q_BS9LmQ&list=UU7aH7HVqDvwB1xNHfSl-fDw

  ----------------------------

  Самый лучший Arduino дисплей OLED LCD Display I2C 0.96 IIC Serial 128X64
  видео: http://www.youtube.com/watch?v=niA3aPu3-dQ&list=UU7aH7HVqDvwB1xNHfSl-fDw
  ============================================


*/

#include <EEPROM.h> // Подключаем библиотеку для записи в энергонезависимую память "Card UID" / уникальный номер NFC карты.
#include <SPI.h>
#include <Wire.h> // Подключаем библиотеку для шины I2C / для работы дисплея.

// ---------------------------------
// Скачать библиотеки для дисплея. https://yadi.sk/d/9F_uW1wIZUDna
#include <Adafruit_GFX.h> // Скачанная библиотека для дисплея.
#include <Adafruit_SSD1306.h> // Скачанная библиотека для дисплея.
// ---------------------------------

// Библиотека "RFID", для RC522 Card Read. Скачать http://yadi.sk/d/6XLMLCuxSjdGa
#include <MFRC522.h> // скачанная библиотека "RFID" для RC522 Card Read Module.
// ****************************************

/*
  Для RC522 Card Read Module. подключение для Arduino Uno и Mega, производится к разным Pin!
  ----------------------------------------------------- Nicola Coppola
   Pin layout should be as follows:
   Signal     Pin              Pin               Pin
              Arduino Uno      Arduino Mega      MFRC522 board
   ------------------------------------------------------------
   Reset      9                5                 RST
   SPI SS     10               53                SDA
   SPI MOSI   11               51                MOSI
   SPI MISO   12               50                MISO
   SPI SCK    13               52                SCK

*/

// Два Pin (SS и RST) допускают произвольное подключение и конфигурируются в коде.
// !(SS - он же - SDA).
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

Adafruit_SSD1306 display(SDA_PIN);

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Объект MFRC522C / reate MFRC522 instance.
unsigned long uidDec, uidDecTemp;  // Для отображения номера карточки в десятичном формате.
// **************************************************************




int userCardCount = 5;

boolean lock = true;

unsigned long CardUIDeEPROMread[userCardCount];

unsigned long adminCardUID = 4198836864;

int ARRAYindexUIDcard; // Индекс для массива CardUIDeEPROMread.

int EEPROMstartAddr; // Стартовая ячейка памяти для записи / чтения EEPROM "Card UID".
// -----------------------------

int LockSwitch; // Замок / Переключатель / Блокиратор.



void setup() {
  // Serial.begin(9600);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // initialize with the I2C addr 0x3C (for the  0.96" 128X64 OLED LCD Display)I2C АДРЕС.

  SPI.begin(); //  инициализация SPI / Init SPI bus.
  mfrc522.PCD_Init(); // инициализация MFRC522 / Init MFRC522 card.

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LOCK_PIN, OUTPUT);
  pinMode(LED_1_PIN, OUTPUT);
  pinMode(LED_2_PIN, OUTPUT);
  pinMode(LED_3_PIN, OUTPUT);

  pinMode(SDA_PIN, OUTPUT);
  pinMode(SCL_PIN, OUTPUT);


  digitalWrite(LOCK_PIN, LOW);

  buzzer(1);
  lockScooter();
  EEPROMreadUIDcard(); // Запускаем функцию, для перезаписи массива CardUIDeEPROMread, данными из EEPROM.
  // Если хотите увидеть в Serial мониторе все записанные № Card-UID, раскомментируйте строку.
  for (int i = 0; i < userCardCount; i++) {
    Serial.print(i);
    Serial.print(" ---- ");
    Serial.println(CardUIDeEPROMread[i]);
  }

  //  for (int i = 0; i < 512; i++)EEPROM.write(i, 0); // EEPROM Clear / Очистить / стереть EEPROM.
}

void loop() {

  // Поиск новой NFC карты / Look for new cards.
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Выбор карточки / Select one of the cards.
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Выдача серийного номера карточки "UID".
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    uidDecTemp = mfrc522.uid.uidByte[i];
    uidDec = uidDec * 256 + uidDecTemp;
  }
  // ----------------------------------------

  // Каждый раз подготавливаем дисплей для записи.
  display.clearDisplay(); // Clear the buffer. / Очистить буфер дисплея.
  display.setTextColor(WHITE); // Цвет текста.
  display.setTextSize(2); // Размер текста (2).
  display.setCursor(0, 0); // Устанавливаем курсор в колонку / Pixel 0, строку / Pixel 0.

  // ----------------------------------------

  // uidDec - Это полученный "Card UID" / уникальный номер NFC карты.

  if (uidDec == adminCardUID || LockSwitch > 0) {
    EEPROMwriteUIDcard(); // Запускаем функцию записи в EEPROM "Card UID" пользователей.
  }
  // ----------------------------------------


  // Для режима управления.
  if (LockSwitch == 0) // только если находимся в "режиме управления".
  {
    // Подготавливаемся для перебора массива CardUIDeEPROMread.
    for (ARRAYindexUIDcard = 0; ARRAYindexUIDcard < userCardCount; ARRAYindexUIDcard++)
    {

      //  Перебираем весь массив CardUIDeEPROMread, Если поднесённая карта совпадёт с одной из 50  переменных массива.
      if (CardUIDeEPROMread[ARRAYindexUIDcard] == uidDec) // Если обнаружено совпадение с поднесенной картой.
      {
        unlockScooter();
        break;
      }
    }

    // ARRAYindexUIDcard == 50 Означает, что мы перебрали весь массив от 0 до 49, и не обнаружили совпадений с поднесённой "Card UID".
    // То есть поднесенная карта отсутствует в базе, выводим на дисплей "не найдена карта" и № "Card UID" карты.
    if (ARRAYindexUIDcard == 50)display.println("NOT  Found CARD-UID"), display.println(uidDec), display.display();
    delay(2000);
    ARRAYindexUIDcard = 0;
    DisplayWAiT_CARD(); // Запускаем функцию, заставка дисплея.
  }
}



void EEPROMwriteUIDcard() {

  if (LockSwitch == 0) // Если находились в режиме управления.
  {
    display.println("   START    RECORD   Card UID   CLIENT");
    display.display();
    buzzer(3);
  }

  // -------

  // Для пропуска записи в ячейку памяти.
  if (LockSwitch > 0) // Если находимся в "режиме записи".
  {
    if (uidDec == adminCardUID) // Если поднесена карта администратора.
    {
      display.println("   SKIP     RECORD   ");
      display.setTextSize(3);
      display.setCursor(40, 40);
      display.println(EEPROMstartAddr / 5); // Выводим № пропущенной ячейки памяти.
      display.display();

      EEPROMstartAddr += 5; // Пропускаем запись в ячейку памяти, если не хотим записывать туда "Card UID".

      buzzer(1);
    } else // Иначе, то есть поднесена карта для записи.

      // "Card UID" / № карты это "длинное число", которое не поместится в одну ячейку памяти EEPROM.
      // Разрубим "длинное число" на 4 части, и кусками, запишем его в 4 ячейки EEPROM. Начинаем запись с адреса EEPROMstartAddr.

    {
      EEPROM.write(EEPROMstartAddr, uidDec & 0xFF);
      EEPROM.write(EEPROMstartAddr + 1, (uidDec & 0xFF00) >> 8);
      EEPROM.write(EEPROMstartAddr + 2, (uidDec & 0xFF0000) >> 16);
      EEPROM.write(EEPROMstartAddr + 3, (uidDec & 0xFF000000) >> 24);
      // Записали!.
      delay(10);
      // --
      display.println("RECORD OK! IN MEMORY ");
      display.setTextSize(3);
      display.setCursor(50, 40);
      display.println(EEPROMstartAddr / 5); // Выводим № записанной ячейки памяти.
      display.display();

      EEPROMstartAddr += 5; // Прибавляем 5 к стартовой ячейки записи.
      buzzer(1);
    }
  }

  LockSwitch++; // Разблокируем режим записи, и заблокируем режим управления.

  if (EEPROMstartAddr / 5 == userCardCount) // если дошли до 5.
  {
    delay(2000);
    display.clearDisplay(); // Clear the buffer. / Очистить буфер.
    display.setTextColor(WHITE); // Цвет текста.
    display.setTextSize(3); // Размер текста (3).
    display.setCursor(0, 0);
    display.println("RECORD FINISH");
    display.display();


    buzzerWithTime(1, 2000);

    EEPROMstartAddr = 0;
    uidDec = 0;
    ARRAYindexUIDcard = 0;

    EEPROMreadUIDcard(); // Запускаем функцию, для перезаписи массива CardUIDeEPROMread, данными из EEPROM.
  }
}

// -------------------------------------------


// Делаем функцию, перезаписи массива CardUIDeEPROMread, данными из EEPROM.
void EEPROMreadUIDcard()
{
  for (int i = 0; i < userCardCount; i++)
  {
    byte val = EEPROM.read(EEPROMstartAddr + 3);
    CardUIDeEPROMread[ARRAYindexUIDcard] = (CardUIDeEPROMread[ARRAYindexUIDcard] << 8) | val;
    val = EEPROM.read(EEPROMstartAddr + 2);
    CardUIDeEPROMread[ARRAYindexUIDcard] = (CardUIDeEPROMread[ARRAYindexUIDcard] << 8) | val;
    val = EEPROM.read(EEPROMstartAddr + 1); // увеличиваем EEPROMstartAddr на 1.
    CardUIDeEPROMread[ARRAYindexUIDcard] = (CardUIDeEPROMread[ARRAYindexUIDcard] << 8) | val;
    val = EEPROM.read(EEPROMstartAddr);
    CardUIDeEPROMread[ARRAYindexUIDcard] = (CardUIDeEPROMread[ARRAYindexUIDcard] << 8) | val;

    ARRAYindexUIDcard++; // увеличиваем на 1.
    EEPROMstartAddr += 5; // увеличиваем на 5.
  }

  ARRAYindexUIDcard = 0;
  EEPROMstartAddr = 0;
  uidDec = 0;
  LockSwitch = 0;
  DisplayWAiT_CARD();
}

void unlockScooter() {
  display.clearDisplay();
  display.print("Разблокирован");
  display.setTextSize(3);
  display.display();

  digitalWrite(LOCK_PIN, HIGH);
  lock = false;

  buzzerWithTime(1, 1000)
  delay(1); // Дёргаем микроконтроллер.
}

void lockScooter() {
  display.clearDisplay();
  display.print("Заблокирован");
  display.setTextSize(3);
  display.display();

  digitalWrite(LOCK_PIN, LOW);
  lock = true;

  buzzerWithTime(1, 1000)
  delay(1); // Дёргаем микроконтроллер.
}

void buzzer(int count) {
  buzzerWithTime(count, 400)
}

void buzzerWithTime(int count, int buzzerTime) {
  for (int i = 0; i < count; i++) {
    delay(400);
    digitalWrite(PiezoPin, HIGH);
    delay(buzzerTime);
    digitalWrite(PiezoPin, LOW);
  }
}
