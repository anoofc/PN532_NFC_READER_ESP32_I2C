#define DEBUG 1

#define COMMAND1 "A"
#define COMMAND2 "B"
#define COMMAND3 "C"
#define COMMAND4 "D"
#define COMMAND5 "E"
#define COMMAND6 "F"
#define COMMAND7 "G"
#define COMMAND8 "H"
#define COMMAND9 "I"
#define COMMAND10 "J"

#define REMOVE_COMMAND "Z" 

#define TIMEOUT 100

#define PN532_IRQ   (2)
#define PN532_RESET (3)  // Not connected by default on the NFC Shield


#include <Arduino.h>

#include <Wire.h>
#include <Adafruit_PN532.h>
#include <BluetoothSerial.h>
#include <EEPROM.h>
// SCK = 13, MOSI = 11, MISO = 12.  The SS line can be any digital IO pin.
//Adafruit_PN532 nfc(PN532_SS);

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

BluetoothSerial SerialBT;

bool success = false;
bool cardPresesnt = false;

uint8_t numTags = EEPROM.read(1);
String commands[] = {COMMAND1, COMMAND2, COMMAND3, COMMAND4, COMMAND5, COMMAND6, COMMAND7, COMMAND8, COMMAND9, COMMAND10};
String tagID      = "";
String prevTagID  = "";

void writeStringToEEPROM(int addrOffset, const String &strToWrite) {
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++) {
      EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
}

String readStringFromEEPROM(int addrOffset) {
    int newStrLen = EEPROM.read(addrOffset);
    char data[newStrLen + 1];
    for (int i = 0; i < newStrLen; i++) {
        data[i] = EEPROM.read(addrOffset + 1 + i);
    }
    data[newStrLen] = '\0';
    return String(data);
}

String tags[] = {readStringFromEEPROM(10), readStringFromEEPROM(20), readStringFromEEPROM(30), readStringFromEEPROM(40), readStringFromEEPROM(50), readStringFromEEPROM(60), readStringFromEEPROM(70), readStringFromEEPROM(80), readStringFromEEPROM(90), readStringFromEEPROM(100)};  


void processTagID(){
  for (int i = 0; i < numTags; i++) {
    if (tagID == tags[i]) {
      Serial.println(commands[i]);
      return;
    }
  }
  if (DEBUG) {Serial.println("UNKNOWN TAG");}
}

void readNFC(){
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  tagID = "";
  // Wait for an NTAG203 card.  When one is found 'uid' will be populated with
  // the UID, and uidLength will indicate the size of the UUID (normally 7)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, TIMEOUT);

  // NO CHANGE IN CARD
  if (success && cardPresesnt){ return; }
  // WAITING FOR NEW CARD
  if (!success && !cardPresesnt) { return; }
  // IF NEW TAG COUND
  if (success & (!cardPresesnt)) {
    cardPresesnt = true;
    // Display some basic information about the card
    // Store UID to tagID
    for (uint8_t i = 0; i < uidLength; i++) {
      if (uid[i] < 0x10) { tagID += "0"; }
      tagID += String(uid[i], HEX);
    }
    tagID.toUpperCase();
    prevTagID = tagID;
    if (DEBUG) {
      Serial.println("Found an ISO14443A card");
      Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
      if (DEBUG) { Serial.println("TAG ID: "+ tagID); }
      // Serial.print("  UID Value: ");
      // nfc.PrintHex(uid, uidLength);
      Serial.println("");
    }
    processTagID();
    return;
  }
  // IF CARD REMOVED
  if (!success && cardPresesnt) {
    cardPresesnt = false;
    if(DEBUG) {Serial.println("CARD REMOVED");}
    for (int i = 0; i < numTags; i++) {
      if (prevTagID == tags[i]) {
        Serial.println(REMOVE_COMMAND);
        return;
      }
    }
  }
}

void nfcInit(){
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  
  if (! versiondata) {
    if (DEBUG) { Serial.println("Didn't find PN53x board");}
    while (1); // halt
  }
  
  if (DEBUG) {
    // Got ok data, print it out!
    Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX);
    Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC);
    Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);

    Serial.println("Waiting for an ISO14443A Card ...");
  } 
}
void processData(String data) {
  if (data.startsWith("N")) {
    numTags = data.substring(1, data.length()).toInt();
    Serial.println(numTags);
    EEPROM.write(0, numTags);
    EEPROM.commit();
    int num = EEPROM.read(0);
    delay(10);
    SerialBT.println("NUM TAGS SET TO: " + String(num));
    return;
  } else if (data.startsWith("T")) {
    int index = data.substring(1, data.length()).toInt() - 1;
    if (index >= 0 && index < 10) {
      tags[index] = prevTagID;
      writeStringToEEPROM(10 + index * 10, prevTagID);
      EEPROM.commit();
    }
    for (int i = 0; i < numTags; i++) {
      String tagID_ = readStringFromEEPROM(10 + i * 10);
      delay(10);
      SerialBT.println("Index: " + String(i) + " TAG ID: " + tagID_);
    } 
    return;
  }
}
void readBTSerial(){
  if (SerialBT.available()) {
    String incoming = SerialBT.readStringUntil('\n');
    processData(incoming);
    if (DEBUG) {SerialBT.println(incoming);}
  }
}

void eepromInit(){
  EEPROM.begin(512);
}

void setup() {
  Serial.begin(9600);
  SerialBT.begin("RFID_PN532");
  eepromInit();
  nfcInit();
}

void loop() {
  readNFC();
  readBTSerial();
}