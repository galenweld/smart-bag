/**************************************************************************/
/*! 
   Clears valid bit without changing anything else.

*/
/**************************************************************************/
// Libraries Needed
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <SparkFunTMP102.h>
#include "SparkFunTMP102.h"
#include <LiquidCrystal.h>

// Pins for the RFID Reader
#define PN532_SCK  (A2)
#define PN532_MOSI (A3)
#define PN532_SS   (2)
#define PN532_MISO (3)
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

// Pins and Setup for for LCD
#define LCD_RS (12)
#define LCD_ENABLE (13)
LiquidCrystal lcd(LCD_RS, LCD_ENABLE, 4, 5, 6, 7, 8, 9, 10, 11); 

// Stuff for thermometer
TMP102 thermometer(0x48);

// Medication Information
// These encode the maximum, and minimum, temp limits for each med.
// Defined in order: Albuterol, Aspirin, Epi, Glucose
const int med_high_temp[] = {80, 100, 100, 100};
const int med_low_temp[] = {0, 0, 32, 0};


// Definitions to make code more readable
#define ALB (0)  // albuterol
#define ASA (1)  // aspirin
#define EPI (2)  // epinepherine
#define GLC (3)  // glucose

#define NOM (0)  // medication is normal
#define EXP (1)  // medication is expired
#define HOT (2)  // medication has exceeded environmental constraints

#define MED_TYPE   (0) // medication type
#define MED_EXP    (1) // medication expiration date
#define MED_STATUS (2) // medication status

// Global Var Defs
int date = 0;
float temp = 70.0;
int medStatus[4] = {NOM, NOM, NOM, NOM}; // Albuterol, Aspirin, Epi, Glucose

void setup(void) {
  thermometer.begin();

  // Set Up LCD
  lcd.begin(16, 2); // number of chars and lines
  lcd.clear();
  lcd.print("Loading");

  // Set Up RFID
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    lcd.clear();
    lcd.print("err-no RFID");
    while (1); // halt
  }
  // Got ok data, print it out!
  lcd.clear();
  lcd.print("RFID Found");
  
  // configure board to read RFID tags
  nfc.SAMConfig();
  
  lcd.clear();
  lcd.print("Tap Card");
}



void loop(void) {
  lcd.clear();
  lcd.print("Tap Card");
  
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    
  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  
  if (success) {
    // Display some basic information about the card
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("card found");
    
    if (uidLength == 4)
    {
      // We probably have a Mifare Classic card ... 
    
      // Now we need to try to authenticate it for read/write access
      // Try with the factory default KeyA: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
      uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    
    // Start with block 4 (the first block of sector 1) since sector 0
    // contains the manufacturer data and it's probably better just
    // to leave it alone unless you know what you're doing
      success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);
    
      if (success)
      {
        uint8_t data[16];
    
        // If you want to write something to block 4 to test with, uncomment
        // the following line and this text should be read back in a minute
        //memcpy(data, med, sizeof data);
        //success = nfc.mifareclassic_WriteDataBlock (4, data);

        // Try to read the contents of block 4
        success = nfc.mifareclassic_ReadDataBlock(4, data);
    
        if (success)
        {
          // Data seems to have been read ... spit it out
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("card tapped");

          data[2] = 0;
          success = nfc.mifareclassic_WriteDataBlock (4, data);

          delay(1000);
          
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("card cleared");
      
          // Wait a bit before reading the card again
          delay(2000);
        }

        else {lcd.clear(); lcd.print("err-block read");}
      }
      else {lcd.clear(); lcd.print("err-auth");}
    }
    else {lcd.clear(); lcd.print("err-uid length");}
  }
}

