/*
    IOT102 Project - Smart Lock
    Thực hiện: SE1730 - Nhóm 7
        - Hoàng Tuấn Anh
        - Trần Văn Hiệp
        - Trần Anh Tuấn
*/

/* Notes:
    - Mật khẩu mặc định 1: 1234 - Lưu ở ô nhớ số 10
    - Mật khẩu mặc định 2: 9999 - Lưu ở ô nhớ số 100
    - UID của RFID mặc định: 0228581a
    - UID của RFID thứ hai - Lưu từ ô nhớ 500 trở đi

    Ở Menu chính:
      - Bấm D để vào chế độ nhập mật khẩu
      - Bấm C để vào chế độ đổi mật khẩu
      - Bấm A để vào chế độ thêm thẻ RFID thứ hai
    Ở các Menu chức năng:
      - Bấm D để trở về hoặc thoát
      - Bấm * để xóa tất cả các số đã nhập tại chức năng nhập mật khẩu
*/

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
const String UIDMaster = "0228581a";
String secondUID;
int delayTimeDefault = 3000;
int attempts = 3;
int changePassAttempts = 3;
int lockMins = 1;
boolean isWarning = false;
int lockDelay = 1000;
int mins = 0;
int secs = 0;

int lastDoorState = 0;
int buzzerPin = 8;
int servoPin = 3;
int resetPassPin = 2;

MFRC522 rfid(SS_PIN, RST_PIN);
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myServo;

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

  // Đọc lại mật khẩu và UID từ EEPROM
  defaultPassword1 = EEPROM.readInt(10);
  defaultPassword2 = EEPROM.readInt(100);
  secondUID = readUIDFromEEPROM(500);
}

void loop() {
  lcd.setCursor(0, 0);
  lcd.print("Smart Lock - GR7");
  // Đọc mã RFID
  scanRFID();

  // Kiểm tra keypad
  char key = keypad.getKey();
  if (key == 'D') { // Thực hiện nhập mật khẩu
    checkPass();
  } else if (key == 'C') { // Thực hiện chức năng đổi mật khẩu
    changePass();
  } else if (key == 'A') { // Thực hiện chức năng đặt thẻ rfid phụ
    addSecondCard();
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
    attempts = 3;
    changePassAttempts = 3;
    if (isWarning == true) { // Thực hiện khi quét RFID trong khi bị khóa nhập mật khẩu
      mins = 0;
      secs = 0;
      attempts = 3;
    }
    isWarning = false;
    lockMins = 1;
    return true;
  }
  return false;
}

void checkPass() {
  if (attempts == 0) { // Thực hiện khi người dùng nhập sai mật khẩu liên tục 3 lần
    attempts = 1;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Try again in:");
    // Đếm ngược, mặc định 1 phút
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

    // Nhân đôi thời gian nếu tiếp tục sai mật khẩu
    if (isWarning == true) {
      lockMins = lockMins * 2;
      if (lockMins > 59) { // Thời gian khóa sẽ luôn bé hơn 60 phút
        lockMins = 59;
      }
    }
    lcd.clear();

  } else { // Thực hiện khi người dùng chưa, hoặc đã nhập sai mật khẩu dưới 2 lần
    int enteredPassword = enterPass("Enter password:");
    Serial.print("[KEYPAD] --> User enterd: ");
    Serial.println(enteredPassword);
    // So sánh mật khẩu trong bộ nhớ và mật khẩu nhập vào
    if (enteredPassword == defaultPassword1 || enteredPassword == defaultPassword2) {
      Serial.println("[SYSTEM] --> Correct password.");
      accessDoor();
      attempts = 3;
      changePassAttempts = 3;
      isWarning = false;
      lockMins = 1;
      delay(delayTimeDefault);
    } else { // Sai mật khẩu
      lcd.clear();
      lcd.print("Wrong password!");
      Serial.println("[SYSTEM] --> Wrong password.");
      attempts--;
      if (attempts == 0) { // Hiển thị sai mật khẩu, bật còi cảnh báo
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
  do { // Tự động kết thúc vòng lặp khi đã nhập đủ 4 số
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
  if (lastDoorState == 0) { // Mở của
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
  } else { // Đóng cửa
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Door closed!");
    myServo.write(90);
    digitalWrite(buzzerPin, HIGH);
    delay(250);
    digitalWrite(buzzerPin, LOW);
  }
  lastDoorState = !lastDoorState;
}

void changePass() {
  lcd.clear();
  int oldPass = 0;
  char key;
  do { // Kết thúc khi người dùng bấm D
    if (changePassAttempts == 0) { // Thực hiện khi người dùng nhập sai mk cũ liên tục 3 lần
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Function locked!");
      warningSound();
      lcd.clear();
      return;
  }

  // Xử lý đổi mật khẩu
  key = keypad.getKey();
  lcd.setCursor(0, 0);
  lcd.print("1. Change pass 1");
  lcd.setCursor(0, 1);
  lcd.print("2. Change pass 2");
  switch (key) {
    case '1': // Đổi mật khẩu 1
      oldPass = enterPass("Old password:");
      if (oldPass == defaultPassword1) { // Thực hiện khi nhập đúng mật khẩu cũ
        int newPass = enterPass("New password:");
        EEPROM.writeInt(10, newPass);
        delay(100);
        defaultPassword1 = EEPROM.readInt(10);
        Serial.println("[PASSWORD] --> Reloaded password.");
        Serial.print("[PASSWORD] --> Changed password 1 to:");
        Serial.println(defaultPassword1);
        lcd.clear();
        lcd.print("Successfully!");
        delay(delayTimeDefault);
      } else { // Thực hiện khi nhập sai mật khẩu
        changePassAttempts--;
        lcd.clear();
        lcd.print("Wrong password!");
        delay(delayTimeDefault);
      }
      break;
    case '2': // Đổi mật khẩu 2
      oldPass = enterPass("Old password:");
      if (oldPass == defaultPassword2) { // Thực hiện khi nhập đúng mật khẩu cũ
        int newPass = enterPass("New password:");
        EEPROM.writeInt(100, newPass);
        delay(100);
        defaultPassword2 = EEPROM.readInt(100);
        Serial.println("[PASSWORD] --> Reloaded password.");
        Serial.print("[PASSWORD] --> Changed password 2 to:");
        Serial.println(defaultPassword2);
        lcd.clear();
        lcd.print("Successfull");
        delay(delayTimeDefault);
      } else { // Thực hiện khi nhập sai mật khẩu
        changePassAttempts--;
        lcd.clear();
        lcd.print("Wrong password!");
        delay(delayTimeDefault);
      }
      break;
  }
} while (key != 'D');
lcd.clear();
}

void addSecondCard() {
  char key;
  lcd.clear();
  do {
    lcd.setCursor(0, 0);
    lcd.print("Add second RFID");
    lcd.setCursor(0, 1);
    lcd.print("Press 'A'");
    key = keypad.getKey();
    if (key == 'A') {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Scanning master");
      lcd.setCursor(0, 1);
      lcd.print("UID card...");
      String givenUID = readUIDNum();
      if (givenUID.equals(UIDMaster)) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Correct card!");
        lcd.setCursor(0, 1);
        lcd.print("Next step...");
        delay(delayTimeDefault);
        String secondCardUID;
        lcd.clear();
        do {
          key = keypad.getKey();
          lcd.setCursor(0, 0);
          lcd.print("Scanning new");
          lcd.setCursor(0, 1);
          lcd.print("UID card...");
          secondCardUID = readUIDNum();
          while (true) { // Kết thúc khi nhập đúng hoặc sai mật khẩu
            int givenPass = enterPass("Enter password:");
            if (givenPass == defaultPassword1 || givenPass == defaultPassword2) { // Thực hiện khi nhập đúng mật khẩu
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Add new card");
              lcd.setCursor(0, 1);
              lcd.print("successfully!");
              Serial.print("Added: ");
              Serial.println(secondUID);
              writeUIDToEEPROM(500, secondCardUID);
              secondUID = readUIDFromEEPROM(500);
              delay(delayTimeDefault);
              lcd.clear();
              return;
            } else { // Thực hiện khi nhập sai mật khẩu
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Wrong password!");
              delay(delayTimeDefault);
              return;
            }
          }
        } while (secondCardUID == "");
      } else { // Thực hiện khi quét sai thẻ
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Wrong card!");
        delay(delayTimeDefault);
        key = 'D';
      }
    }
  } while (key != 'D');
  lcd.clear();
}

String readUIDNum() {
  String readingUID = "";
  while (true) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) { // Thực hiện khi có thẻ đưa vào quét

      for (byte i = 0; i < rfid.uid.size; i++) {
        readingUID.concat(String(rfid.uid.uidByte[i] < 0x10 ? "0" : ""));
        readingUID.concat(String(rfid.uid.uidByte[i], HEX));
      }
      scanRFIDSound();
      if (readingUID != "") { // Thực hiện khi đã đọc được UID của thẻ
        return readingUID;
      }
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }
  }
}

String readUIDFromEEPROM(int addrOffset) {
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen + 1];
  for (int i = 0; i < newStrLen; i++)
  {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0';
  return String(data);
}

void writeUIDToEEPROM(int addrOffset, const String &strToWrite)
{
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
}

void resetPass() {
  EEPROM.writeInt(10, 1234);
  delay(100);
  EEPROM.writeInt(100, 9999);
  delay(100);
  Serial.println("[PASSWORD] --> Password reseted.");
  defaultPassword1 = EEPROM.readInt(10);
  delay(100);
  defaultPassword2 = EEPROM.readInt(100);
  delay(100);
  Serial.println("[PASSWORD] --> Reloaded password.");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Reset password");
  lcd.setCursor(0, 1);
  lcd.print("successfull");
  digitalWrite(buzzerPin, HIGH);
  delay(2000);
  digitalWrite(buzzerPin, LOW);
  lcd.clear();
}

void scanRFIDSound() {
  digitalWrite(buzzerPin, HIGH);
  delay(500);
  digitalWrite(buzzerPin, LOW);
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
