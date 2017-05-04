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
#define PN532_SCK  (A2) //0
#define PN532_MOSI (A3) //1
#define PN532_SS   (2)
#define PN532_MISO (3)
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

// Pins and Setup for for LCD
#define LCD_RS (12)
#define LCD_ENABLE (13)
LiquidCrystal lcd(LCD_RS, LCD_ENABLE, 4, 5, 6, 7, 8, 9, 10, 11); 

// Pin for LED
#define LED_OUT (A0)

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
  Serial.begin(9600);
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
  lcd.print("Welcome");
}


void loop(void) {
  // Get Temp
  thermometer.wakeup();
  temp = thermometer.readTempF();
  thermometer.sleep();

  // Report Status
  report_status();
  debug_med_status();

  // Date and Temp
  lcd.setCursor(0,1);lcd.print(temp);lcd.print('F');

  lcd.setCursor(14,1);lcd.print(date); // offset printing

  // Look for a Card
  find_and_update_card();

  // Notify User that Their Card Has Been Read
  flashLight();

  // Increment the date
  date++;

  // Wait a bit before reading the card again
  delay(1000);

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
    
    if (uidLength == 4) {
      // Try with the factory default KeyA: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
      uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
      success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);
    
      if (success)
      {
        uint8_t data[16];

        Serial.println("card read");

        // Try to read the contents of block 4
        success = nfc.mifareclassic_ReadDataBlock(4, data);
    
        if (success)
        {
          // Call Helper          
          med_handler((int)data[MED_TYPE], (int)data[MED_EXP], (int)data[MED_STATUS]);
      
        }
        else {lcd.clear(); lcd.print("err-block read");}
      }
      else {lcd.clear(); lcd.print("err-auth");}
    }
    else {lcd.clear(); lcd.print("err-uid length");}
  }
}

void report_status(void) {
  // Reads the status from global variables and updates information on display.
  
  // Med Status
  if      (medStatus[ALB] != NOM) print_top_line(ALB, medStatus[ALB]);
  else if (medStatus[ASA] != NOM) print_top_line(ASA, medStatus[ASA]);
  else if (medStatus[EPI] != NOM) print_top_line(EPI, medStatus[EPI]);
  else if (medStatus[GLC] != NOM) print_top_line(GLC, medStatus[GLC]);
  else {lcd.clear(); lcd.setCursor(0,0); lcd.print("SmartBag- normal");}
}

void print_top_line(int m, int r) {
  // prints the top line of the status based on given med and reason
  lcd.clear();
  lcd.setCursor(0,0);

  if (m==ALB) lcd.print("Albuterol "); 
  if (m==ASA) lcd.print("Aspirin   ");
  if (m==EPI) lcd.print("EpiPen    ");
  if (m==GLC) lcd.print("Glucose   ");

  if (r>2)    lcd.print(" bad");
  if (r==EXP) lcd.print(" exp");
  if (r==HOT) lcd.print("temp");
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

  Serial.print("med ");Serial.println(med);
  
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
  Serial.print(medStatus[ALB]);Serial.print(":");
  Serial.print(medStatus[ASA]);Serial.print(":");
  Serial.print(medStatus[EPI]);Serial.print(":");
  Serial.println(medStatus[GLC]);
}

void flashLight(){
  digitalWrite(LED_OUT, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(500);                    // wait for a second
  digitalWrite(LED_OUT, LOW);    // turn the LED off by making the voltage LOW
}
