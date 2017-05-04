/**************************************************************************/
/*! 
    Protoype for project.

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
#define PN532_SCK  (0)
#define PN532_MOSI (1)
#define PN532_SS   (2)
#define PN532_MISO (3)
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

// Pins and Setup for for LCD
#define LCD_RS (12)
#define LCD_ENABLE (13)
const int d0 = 4; const int d1 = 5;
const int d2 = 6; const int d3 = 7;
const int d4 = 8; const int d5 = 9;
const int d6 = 10; const int d7 = 11;
LiquidCrystal lcd(LCD_RS, LCD_ENABLE, 4, 5, 6,7, 8, 9, 10, 11); 

// Stuff for thermometer
TMP102 thermometer(0x48);

// Medication Information
// These encode the maximum, and minimum, temp limits for each med.
// Defined in order: Albuterol, Aspirin, Epi, Glucose
const int med_high_temp[] = {80, 100, 100, 100};
const int med_low_temp[] = {0, 0, 32, 0};


// Definitions to make code more readable
#define ALB (0)  // albuterol
#define ASA (2)  // aspirin
#define EPI (3)  // epinepherine
#define GLC (4)  // glucose

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
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("SmartBag");

  // Set Up RFID
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    lcd.clear();
    lcd.print("Err-No RFID");
    while (1); // halt
  }
  // Got ok data, print it out!
  lcd.clear();
  lcd.print("RFID Found");
  
  // configure board to read RFID tags
  nfc.SAMConfig();
  
  lcd.clear();
  lcd.print("Welcome");
}


void loop(void) {
  // Get Temp
  thermometer.wakeup();
  temp = thermometer.readTempF();
  thermometer.sleep();

  // Report Status
  report_status();

  // Look for a Card
  find_and_update_card();

  // Increment the date
  date++;

}

void find_and_update_card(void) {
  // Waits until a card is found, reads its info, determines if the med is ok, and then
  // updates the card and the global medStatus and delays.

  
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    
  // wait until card is scanned for uid and uidLength to populate
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  
  if (success) {
    //Serial.println("Found a card");
    
    if (uidLength == 4) {
      // Try with the factory default KeyA: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
      //Serial.println("Trying to authenticate block 4 with default KEYA value");
      uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
      success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);
    
      if (success)
      {
        //Serial.println("Sector 1 (Blocks 4..7) has been authenticated");
        uint8_t data[16];

        // Try to read the contents of block 4
        success = nfc.mifareclassic_ReadDataBlock(4, data);
    
        if (success)
        {
          // Call Helper          
          med_handler((int)data[MED_TYPE], (int)data[MED_EXP], (int)data[MED_STATUS]);
      
          // Wait a bit before reading the card again
          delay(1000);
        }
        else {Serial.println("Ooops ... unable to read the requested block.  Try another key?");}
      }
      else {Serial.println("Ooops ... authentication failed: Try another key?");}
    }
    else {Serial.println("Card UID of improper length - error.");}
  }
}

void report_status(void) {
  // Reads the status from global variables and updates information on display.
  
  // Med Status
  if (medStatus[ALB] != NOM) {
    if (medStatus[ALB] == EXP) Serial.println("LCD: Albuterol Expired");
    if (medStatus[ALB] == HOT) Serial.println("LCD: Albuterol out of temp");
  }
  
  else if (medStatus[ASA] != NOM) {
    if (medStatus[ASA] == EXP) Serial.println("LCD: Aspirin Expired");
    if (medStatus[ASA] == HOT) Serial.println("LCD: Aspirin out of temp");
  }
  else if (medStatus[EPI] != NOM) {
    if (medStatus[EPI] == EXP) Serial.println("LCD: EpiPen Expired");
    if (medStatus[EPI] == HOT) Serial.println("LCD: EpiPen out of temp");
  }
  else if (medStatus[GLC] != NOM) {
    if (medStatus[GLC] == EXP) Serial.println("LCD: Glucose Expired");
    if (medStatus[GLC] == HOT) Serial.println("LCD: Glucose out of temp");
  }
  else {Serial.println("LCD: System Normal");}

  // Date and Temp
  Serial.print("LCD: Temperature: ");
  Serial.println(temp);

  Serial.print("LCD: Date: ");
  Serial.println(date);
  debug_med_status();
  Serial.println("");
}


void med_handler(int m, int e, int v) {
  // universal med handler. takes med, exp, and valid
  // updates and invalidates as needed
  if (date >= e || v == EXP) invalidate_med(m, e, EXP);
  else if (temp > med_high_temp[m] || v == HOT) invalidate_med(m, e, HOT);
  else if (temp < med_low_temp[m] || v == HOT) invalidate_med(m, e, HOT);

  else medStatus[m] = NOM;
  }


// Medication Updates
void invalidate_med(int med, int e, int reason) {
  // immediately invalidates the scanned medication by updating its
  // tag with the given reason while preserving other info
  // then, updates system status as well
  
  uint8_t data[16];
  uint8_t success;
  uint8_t bad_med[16] = { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  bad_med[MED_TYPE] = med;
  bad_med[MED_STATUS] = reason;
  bad_med[MED_EXP] = e;
  memcpy(data, bad_med, sizeof data);
  success = nfc.mifareclassic_WriteDataBlock (4, data);

  medStatus[med] = reason;

  
}


void debug_med_status(void) {
  delay(10);
  Serial.print(medStatus[ALB]);Serial.print(":");
  Serial.print(medStatus[ASA]);Serial.print(":");
  Serial.print(medStatus[EPI]);Serial.print(":");
  Serial.println(medStatus[GLC]);
  delay(10);
}

