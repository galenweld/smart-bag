/**************************************************************************/
/*! 
    Protoype for project. Outputs info over serial connection instead
    of LCD.

*/
/**************************************************************************/
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <SparkFunTMP102.h>
#include "SparkFunTMP102.h"

// Pins for the RFID Reader
#define PN532_SCK  (2)
#define PN532_MOSI (3)
#define PN532_SS   (4)
#define PN532_MISO (5)
#define PN532_IRQ   (2)
#define PN532_RESET (3)  // Not connected by default on the NFC Shield
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);


// Stuff for thermometer
TMP102 thermometer(0x48);

// Definitions to make code more readable
#define ALB (0)  // albuterol
#define ASA (2)  // aspirin
#define EPI (3)  // epinepherine
#define GLC (4)  // glucose

#define NOM (0)  // medication is normal
#define EXP (1)  // medication is expired
#define HOT (2)  // medication has exceeded environmental constraints

// Global Var Defs
float temp;
int medStatus[] = {NOM, NOM, NOM, NOM}; // Albuterol, Aspirin, Epi, Glucose
int date = 0;

void setup(void) {
  Serial.begin(9600);
  thermometer.begin();

  // Set Up RFID
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  
  // configure board to read RFID tags
  nfc.SAMConfig();
  
  Serial.println("Finished Config, Waiting For Cards");
}


void loop(void) {
  // Get Temp
  thermometer.wakeup();
  temp = thermometer.readTempF();
  thermometer.sleep();

  // Report Status
  report_status();

  // Look for a Card
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    
  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  
  if (success) {
    Serial.println("Found a card");
    
    if (uidLength == 4)
    {
      // Try with the factory default KeyA: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
      Serial.println("Trying to authenticate block 4 with default KEYA value");
      uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
      success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);
    
      if (success)
      {
        Serial.println("Sector 1 (Blocks 4..7) has been authenticated");
        uint8_t data[16];

        // Try to read the contents of block 4
        success = nfc.mifareclassic_ReadDataBlock(4, data);
    
        if (success)
        {
          // Dispatch to the appropriate helper
          Serial.println("Determining Med Type");
          int med_type = (int) data[0];
          int expires = (int) data[1];
          int valid = (int) data[2];
          
          if (med_type == 0) albuterol(expires, valid);
          if (med_type == 1) aspirin(expires, valid);
          if (med_type == 2) epi(expires, valid);
          if (med_type == 3) glucose(expires, valid);
          
          // bad meds are given type = 5, they are ignored
          if (med_type > 3) Serial.println("found a bad med");
      
          // Wait a bit before reading the card again
          delay(1000);
        }
        else
        {
          Serial.println("Ooops ... unable to read the requested block.  Try another key?");
        }
      }
      else
      {
        Serial.println("Ooops ... authentication failed: Try another key?");
      }
    }
    
    if (uidLength != 4)
    {
      Serial.println("Card UID of improper length error.");

    }
  }
  date++; // inc date
}

void report_status(void) {
  // Reads the status from global variables and updates information on display.
  
  // Med Status
  if (medStatus[0]==0 && medStatus[1]==0 && medStatus[2]==0 && medStatus[3]==0) {
    Serial.println("LCD: System Normal");
  }
  else if (medStatus[0] != 0) {
    // bad albuterol
    if (medStatus[0] == 1) Serial.println("LCD: Albuterol Expired");
    if (medStatus[0] == 2) Serial.println("LCD: Albuterol out of temp");
  }
  
  else if (medStatus[1] != 0) {
    // bad aspirin
    if (medStatus[1] == 1) Serial.println("LCD: Aspirin Expired");
    if (medStatus[1] == 2) Serial.println("LCD: Aspirin out of temp");
  }
  else if (medStatus[2] != 0) {
    // bad epi
    if (medStatus[2] == 1) Serial.println("LCD: EpiPen Expired");
    if (medStatus[2] == 2) Serial.println("LCD: EpiPen out of temp");
  }
  else if (medStatus[3] != 0) {
    // bad glucose
    if (medStatus[3] == 1) Serial.println("LCD: Glucose Expired");
    if (medStatus[3] == 2) Serial.println("LCD: Glucose out of temp");
  }

  // Date and Temp
  Serial.print("LCD: Temperature: ");
  Serial.println(temp);

  Serial.print("LCD: Date: ");
  Serial.println(date);
  Serial.println("");

  // DEBUGGING
  Serial.println("meds");
  Serial.print(medStatus[0]);Serial.print(medStatus[1]);Serial.print(medStatus[2]);Serial.println(medStatus[3]);
  
}


// Medication handlers
void med_handler(int m, int e, int v) {
  // universal med handler. takes med, exp, and valid
  // updates and invalidates as needed
}

void albuterol(int e, int v) {
  Serial.println("Found Albuterol");
  if (date > e || v == 1) {
    // med is expired
    invalidate_med(0, 1);
    Serial.println("expired");
  }
  else if (temp > 80 || v == 2) {
    // med too hot
    invalidate_med(0, 2);
    Serial.println("hot");
  }
  else {
    Serial.println("med ok");
    medStatus[0] = 0;
    }

    Serial.print("exp");Serial.println(e);
    Serial.print("val");Serial.println(v);
}

void aspirin(int e, int v) {
  Serial.println("Found Aspirin");
}

void epi(int e, int v) {
  Serial.println("Found EpiPen");
}

void glucose(int e, int v) {
  Serial.println("Found Glucose");
}

// Medication Updates
void invalidate_med(int med, int reason) {
  // immediately erases the scanned medication and invalidates it
  // then, updates system status for the given med with the given reason
  
  uint8_t data[16];
  uint8_t success;
  uint8_t bad_med[16] = { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  bad_med[0] = med; bad_med[2] = reason;
  memcpy(data, bad_med, sizeof data);
  success = nfc.mifareclassic_WriteDataBlock (4, data);

  medStatus[med] = reason;

  
}

