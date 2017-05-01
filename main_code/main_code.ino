#include <Adafruit_PN532.h>
#include <LiquidCrystal.h>
#include <SparkFunTMP102.h>
#include "SparkFunTMP102.h"

#include <Wire.h>
#include <SPI.h>
//pins for RFID chip
const int PN532_SCK = 0; const int PN532_MOSI = 1;
const int PN532_SS = 2; const int PN532_MISO = 3;
//pins for LCD Display
const int rs = 12; const int enable = 13;
const int d0 = 4; const int d1 = 5;
const int d2 = 6; const int d3 = 7;
const int d4 = 8; const int d5 = 9;
const int d6 = 10; const int d7 = 11;
//pins for temperature sensor
const int ALERT_PIN = A3;
//pin for LED
const int ledOut = A0;

LiquidCrystal lcd(rs, enable, d0, d1, d2, d3, d4, d5, d6, d7); 
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
TMP102 sensor0(0x48);

void setup() {
  //LCD Display
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("Hello!");
  //RFID Sensor
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    while (1); // halt
  }
  nfc.SAMConfig();
  //LED
  pinMode(ledOut, OUTPUT);
  //Temperature Sensor
  pinMode(ALERT_PIN,INPUT);
  sensor0.begin();
  sensor0.setAlertPolarity(1);
  sensor0.setAlertMode(0);
  sensor0.setConversionRate(0);
  sensor0.setExtendedMode(1);
}

void loop() {
  float temperature;
  boolean alertPinState, alertRegisterState;
  sensor0.wakeup();
  lcd.clear();
  temperature = sensor0.readTempC();

  //lcd.print(temperature);
  //lcd.print(" Degrees");
  
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  if (success) {
    nfc.PrintHex(uid, uidLength);
    if (uidLength == 4) {
      // We probably have a Mifare Classic card ... 
      uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
      success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);
      lcd.clear();
      lcd.print("Classic");
      
      if (success) {
        Serial.println("Sector 1 (Blocks 4..7) has been authenticated");
        uint8_t data[16];
        success = nfc.mifareclassic_ReadDataBlock(4, data);
        if (success) {
          lcd.setCursor(0, 1);
          lcd.print("Authenticated");
          
          digitalWrite(A0, HIGH);   // turn the LED on (HIGH is the voltage level)
          delay(2000);                       // wait for a second
          digitalWrite(A0, LOW);    // turn the LED off by making the voltage LOW
          delay(2000);                       // wait for a second
          
          nfc.PrintHexChar(data, 16);
          delay(1000);
        }
        else {
          lcd.setCursor(0, 1);
          lcd.print("Failed");
          delay(1000);
        }
      }
      else {
        lcd.setCursor(0, 1);
        lcd.print("Failed");
      }
    }
    if (uidLength == 7) {
      // We probably have a Mifare Ultralight card ...
      lcd.clear();
      lcd.print("Ultralight");

      uint8_t data[32];
      success = nfc.mifareultralight_ReadPage (4, data);
      if (success) {
        nfc.PrintHexChar(data, 4);
        lcd.setCursor(0, 1);
        lcd.print("Authenticated");
        
        digitalWrite(A0, HIGH);   // turn the LED on (HIGH is the voltage level)
        delay(2000);                       // wait for a second
        digitalWrite(A0, LOW);    // turn the LED off by making the voltage LOW
        delay(2000);                       // wait for a second
          
        delay(1000);
      }
      else {
        lcd.setCursor(0, 1);
        lcd.print("Failed");
        delay(1000);
      }
    }
  }
}
