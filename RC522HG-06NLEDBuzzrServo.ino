#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

#define RST_PIN 9
#define SS_PIN 10 // RFID-RC522 모듈의 SS 핀 번호 (Slave Select)
#define NEOPIXEL_PIN 3
#define LED_COUNT 8
#define BUZZER_PIN 2

Servo SG90;
MFRC522 rc522(SS_PIN, RST_PIN);
Adafruit_NeoPixel neopixel(LED_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial BtSerial(7, 5); // HC-06 모듈의 Rx 및 Tx 핀 번호

const int sg90Pin = 6;

byte registeredClassicCardUID[] = {0x82, 0x2C, 0x3B, 0x30};
byte registeredUltralightCardUID[][7] = {
  {0x04, 0x5C, 0x62, 0x1A, 0xDE, 0x6C, 0x81},
  {0x04, 0x6D, 0x62, 0x1A, 0xDE, 0x6C, 0x81}
  // 추가 UID를 여기에 추가로 작성하세요
};

unsigned long accessTime = 0;
int accessCount = 0;

void printTime(unsigned long time, bool newline = false) {
  unsigned long seconds = time / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;

  seconds = seconds % 60;
  minutes = minutes % 60;

  if (hours < 10) BtSerial.print("0");
  BtSerial.print(hours);
  BtSerial.print(":");
  if (minutes < 10) BtSerial.print("0");
  BtSerial.print(minutes);
  BtSerial.print(":");
  if (seconds < 10) BtSerial.print("0");
  BtSerial.print(seconds);

  if (newline) {
    BtSerial.println();
  }
}

void setup() {
  Serial.begin(9600);
  SPI.begin();
  rc522.PCD_Init();

  SG90.attach(sg90Pin);
  neopixel.begin();

  lcd.init();
  lcd.backlight();
  lcd.begin(16, 2);  // 2행 16열로 LCD 초기화

  pinMode(BUZZER_PIN, OUTPUT);

  BtSerial.begin(9600); // HC-06 모듈의 시리얼 통신 속도 설정
}

void loop() {
  neopixel.clear();
  neopixel.show();

  if (!rc522.PICC_IsNewCardPresent() || !rc522.PICC_ReadCardSerial()) {
    delay(500);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("    Tag Here     ");
    lcd.setCursor(0, 1);
    lcd.print("Ready");
    return;
  }

  lcd.clear();

  if (isRegisteredCard(rc522.uid)) {
    lcd.setCursor(0, 0);
    lcd.print("Access Granted");
    lcd.setCursor(0, 1);
    printCardUID(rc522.uid);  // 변환된 UID 출력
    BtSerial.print("Access Granted - Time: ");
    printTime(accessTime);
    BtSerial.print(" Count: ");
    BtSerial.print(accessCount);
    BtSerial.println();
    SG90.write(90);
    setColor(0, 255, 0);
    playBuzzer(200);
    delay(1000);
    SG90.write(0);
    setColor(0, 0, 0);
    recordAccess(true);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Access Denied");
    lcd.setCursor(0, 1);
    printCardUID(rc522.uid);  // 변환된 UID 출력
    BtSerial.print("Access Denied - Time: ");
    printTime(accessTime);
    BtSerial.print(" Count: ");
    BtSerial.print(accessCount);
    BtSerial.println();
    setColor(255, 0, 0);
    playBuzzer(100);
    delay(500);
    setColor(0, 0, 0);
    recordAccess(false);
  }

  delay(100);
}


void setColor(byte red, byte green, byte blue) {
  for (int i = 0; i < LED_COUNT; i++) {
    neopixel.setPixelColor(i, red, green, blue);
  }
  neopixel.show();
}

bool isRegisteredCard(MFRC522::Uid uid) {
  if (uid.size == sizeof(registeredClassicCardUID)) {
    for (byte i = 0; i < uid.size; i++) {
      if (uid.uidByte[i] != registeredClassicCardUID[i]) {
        return false;
      }
    }
    return true;
  } else {
    for (int j = 0; j < sizeof(registeredUltralightCardUID) / sizeof(registeredUltralightCardUID[0]); j++) {
      bool match = true;
      for (byte i = 0; i < uid.size; i++) {
        if (uid.uidByte[i] != registeredUltralightCardUID[j][i]) {
          match = false;
          break;
        }
      }
      if (match) {
        return true;
      }
    }
    return false;
  }
}


void recordAccess(bool granted) {
  accessTime = millis(); // 현재 시간 기록
  accessCount++; // 카운트 증가
  Serial.print("Access Count: ");
  Serial.print(accessCount);
  Serial.print(" - Time: ");
  printTime(accessTime, true);
  Serial.println();
}

void playBuzzer(int duration) {
  if (isRegisteredCard(rc522.uid)) {
    // 등록된 카드인 경우
    tone(BUZZER_PIN, 1000, duration);
    delay(duration);
    noTone(BUZZER_PIN);
    delay(100); // 울린 후 잠시 기다림
  } else {
    // 등록되지 않은 카드인 경우
    for (int i = 0; i < 2; i++) { // 두 번 울리도록 반복
      tone(BUZZER_PIN, 1500, duration * 2); // 길게 한 번 울림
      delay(duration * 1);
      noTone(BUZZER_PIN);
      delay(100); // 울린 후 잠시 기다림
    }
  }
}

void printCardUID(MFRC522::Uid uid) {
  lcd.print("ID: ");
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) {
      lcd.print("0");
    }
    lcd.print(uid.uidByte[i], HEX);
    BtSerial.print(uid.uidByte[i], HEX);  // Add this line to print UID via Bluetooth
  }
}

