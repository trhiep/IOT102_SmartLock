#include <EEPROMex.h>
#include <EEPROMVar.h>
#include <MFRC522.h>
#include <Servo.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>

#define RST_PIN 9
#define SS_PIN 10

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {7, 6, 5, 4};    // Kết nối các hàng từ chân 9 đến 6 trên Arduino
byte colPins[COLS] = {A3, A2, A1, A0};    // Kết nối các cột từ chân 5 đến 2 trên Arduino

int defaultPassword1 = 1234;
int defaultPassword2 = 9999;
int systemPassword1;
int systemPassword2;
String UIDMaster = "0228581a";
String secondUID = "1f07d0bd";
int delayTimeDefault = 3000;

int attempts = 3;
int lockMins = 1;
boolean isWarning = false;
int lockDelay = 1000;
int mins = 0;
int secs = 0;

int lastDoorState = 0;
int buzzerPin = 8;
int resetPassPin = 2;

MFRC522 rfid(SS_PIN, RST_PIN);
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Địa chỉ I2C và kích thước màn hình LCD (16x2)
Servo myServo;
int servoPin = 3;

void setup() {
  // Setup serial port
  Serial.begin(9600);

  // Setup RFID
  SPI.begin();
  rfid.PCD_Init();

  // Setup servo
  myServo.attach(servoPin);

  // Setup lcd
  lcd.init(); // Khởi động màn hình LCD
  lcd.backlight();

  // Setup các tín hiệu digital
  pinMode(resetPassPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);

  // Setup EEPROM để lưu mật khẩu
  systemPassword1 = EEPROM.readInt(10);
  systemPassword2 = EEPROM.readInt(100);
}

void loop() {
  lcd.setCursor(0, 0);
  lcd.print("Smart Lock - GR7");
  // Đọc mã RFID
  scanRFID();

  // Kiểm tra keypad
  char key = keypad.getKey();
  if (key == 'D') {
    checkPass();
  } else if (key == 'C') {
    changePass();
  }

  // Kiểm tra tín hiệu reset pass
  if (digitalRead(resetPassPin) == LOW) {
    resetPass();
  }
}


void scanRFID() {
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    // Lấy thông tin mã RFID
    String scannedUID = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      scannedUID.concat(String(rfid.uid.uidByte[i] < 0x10 ? "0" : ""));
      scannedUID.concat(String(rfid.uid.uidByte[i], HEX));
    }

    Serial.print("[RFID] --> Scanned UID: ");
    Serial.println(scannedUID);
    // Kiểm tra mã RFID
    if (checkRFID(scannedUID)) {
      accessDoor();
      delay(delayTimeDefault);
      lcd.clear();
      lcd.print("Smart Lock - GR7");

    } else {
      lcd.clear();
      lcd.print("Wrong RFID card!");
      delay(delayTimeDefault);
      lcd.clear();
      lcd.print("Smart Lock - GR7");
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}


boolean checkRFID(String scannedUID) {
  // Kiểm tra mã RFID với danh sách mã được chấp nhận
  if (scannedUID.equals(UIDMaster) || scannedUID.equals(secondUID)) {
    Serial.println("[SYSTEM] --> Correct UID.");
    attempts = 3;
    if (isWarning == true) {
      mins = 0;
      secs = 0;
    }
    isWarning = false;
    lockMins = 1;
    return true;
  }
  Serial.println("[SYSTEM] --> Wrong UID.");
  return false;
}

void checkPass() {
  if (attempts == 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Try again in:");
    for (mins = lockMins - 1; mins >= 0; mins--) {
      for (secs = 59; secs >= 0; secs--) {
        lcd.setCursor(5, 1);
        lcd.print(mins);
        lcd.setCursor(7, 1);
        lcd.print(":");
        lcd.setCursor(8, 1);
        lcd.print(secs);
        delay(lockDelay);
        scanRFID();
        lcd.setCursor(0, 1);
        lcd.print("                ");
      }
    }

    if (isWarning == true) {
      lockMins = lockMins * 2;
      if (lockMins > 59) {
        lockMins = 59;
      }
    }
    attempts = 1;
    lcd.clear();

  } else {
    int enteredPassword = enterPass("Enter password:");
    Serial.print("[KEYPAD] --> User enterd: ");
    Serial.println(enteredPassword);
    if (enteredPassword == systemPassword1 || enteredPassword == systemPassword2) {
      Serial.println("[SYSTEM] --> Correct password.");
      accessDoor();
      attempts = 3;
      isWarning = false;
      lockMins = 1;
      delay(delayTimeDefault);
    } else {
      lcd.clear();
      lcd.print("Wrong password!");
      Serial.println("[SYSTEM] --> Wrong password.");
      attempts--;
      if (attempts == 0) {
        isWarning = true;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Disable unlock");
        lcd.setCursor(0, 1);
        lcd.print("by password!");
        warningSound();
      }
      delay(delayTimeDefault);
      lcd.clear();
    }
  }

}

int enterPass(String msg) {
  int password = 0;
  char key;
  int count = 0;
  lcd.clear();
  lcd.print(msg);
  do {
    key = keypad.getKey();
    if (key >= '0' && key <= '9') {
      enterPassSound();
      password = password * 10 + (key - '0');
      lcd.setCursor(count, 1);
      lcd.print("*");
      count++;
    } else if (key == '*') {
      password = 0;
      count = 0;
      lcd.clear();
      lcd.print(msg);
    }
  } while (count != 4);
  return password;
}

void accessDoor() {
  if (lastDoorState == 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Door opened!");
    myServo.write(0);
    digitalWrite(buzzerPin, HIGH);
    delay(100);
    digitalWrite(buzzerPin, LOW);
    delay(150);
    digitalWrite(buzzerPin, HIGH);
    delay(100);
    digitalWrite(buzzerPin, LOW);
    Serial.println("[SYSTEM] --> Door opened!");
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Door closed!");
    myServo.write(90);
    digitalWrite(buzzerPin, HIGH);
    delay(250);
    digitalWrite(buzzerPin, LOW);
    Serial.println("[SYSTEM] --> Door closed!");
  }
  lastDoorState = !lastDoorState;
}

void changePass() {
  lcd.clear();
  int oldPass = 0;
  char key;
  do {
    key = keypad.getKey();

    lcd.setCursor(0, 0);
    lcd.print("1. Change pass 1");
    lcd.setCursor(0, 1);
    lcd.print("2. Change pass 2");
    switch (key) {
      case '1':
        oldPass = enterPass("Old password:");
        if (oldPass == systemPassword1) {
          int newPass = enterPass("New password:");
          EEPROM.writeInt(10, newPass);
          delay(100);
          systemPassword1 = EEPROM.readInt(10);
          Serial.println("[SYSTEM] --> Reloaded password.");
          Serial.print("[SYSTEM] --> Changed password 1 to:");
          Serial.println(systemPassword1);
          lcd.clear();
          lcd.print("Successfully!");
          delay(delayTimeDefault);
        } else {
          lcd.clear();
          lcd.print("Wrong password!");
          delay(delayTimeDefault);
        }
        break;
      case '2':
        oldPass = enterPass("Old password:");
        if (oldPass == systemPassword2) {
          int newPass = enterPass("New password:");
          EEPROM.writeInt(100, newPass);
          delay(100);
          systemPassword2 = EEPROM.readInt(100);
          Serial.println("[SYSTEM] --> Reloaded password.");
          Serial.print("[SYSTEM] --> Changed password 2 to:");
          Serial.println(systemPassword2);
          lcd.clear();
          lcd.print("Successfull");
          delay(delayTimeDefault);
        } else {
          lcd.clear();
          lcd.print("Wrong password!");
          delay(delayTimeDefault);
        }
        break;
    }
  } while (key != 'D');
  lcd.clear();
}

void resetPass() {

  EEPROM.writeInt(10, defaultPassword1);
  delay(100);
  EEPROM.writeInt(100, defaultPassword2);
  delay(100);
  Serial.println("[SYSTEM] --> Password reseted.");

  systemPassword1 = EEPROM.readInt(10);
  delay(100);
  systemPassword2 = EEPROM.readInt(100);
  delay(100);
  Serial.println("[SYSTEM] --> Reloaded password.");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Reset password");
  lcd.setCursor(0, 1);
  lcd.print("successfull");
  delay(delayTimeDefault);
  lcd.clear();
}

void enterPassSound() {
  digitalWrite(buzzerPin, HIGH);
  delay(20);
  digitalWrite(buzzerPin, LOW);
}

void warningSound() {
  digitalWrite(buzzerPin, HIGH);
  delay(1000);
  digitalWrite(buzzerPin, LOW);
  delay(100);
  digitalWrite(buzzerPin, HIGH);
  delay(1000);
  digitalWrite(buzzerPin, LOW);
  delay(100);
  digitalWrite(buzzerPin, HIGH);
  delay(1000);
  digitalWrite(buzzerPin, LOW);
  delay(100);
  digitalWrite(buzzerPin, HIGH);
  delay(1000);
  digitalWrite(buzzerPin, LOW);
  delay(100);
  digitalWrite(buzzerPin, HIGH);
  delay(1000);
  digitalWrite(buzzerPin, LOW);
}
