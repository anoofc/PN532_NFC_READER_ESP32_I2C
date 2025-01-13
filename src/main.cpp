/**
 * @file    main.cpp
 * @brief   RFID Cube Podium PN532 Firmware
 * @author  Anoof Chappangathil
 * @version 1.0
 * @date    20/12/2024
 * 
 * This is the firmware for the RFID Cube Podium using PN532 NFC Module. 
 * This firmware is used to read NFC tags and execute commands based on the tag ID.
 * Configuration of the tags and commands can be done via Serial or Bluetooth Serial.
 *    - N<num> - Set number of tags. 'Eg: N10'
 *    - T<index> - Set Last placed tag ID for index. Eg: T1
 *    - C<index><command> - Set command for index. Eg: C1HELLO - Set HELLO command for index 1
 *    - R<command> - Set Tag Remove command. Eg: RREMOVED - Set REMOVED command for tag remove
 *    - HELP - Get help
 * 
 */

#define DEBUG       0

#define TIMEOUT     100

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

bool success      = false;
bool cardPresesnt = false;
bool mode         = 0;                               // Mode of operation
uint8_t numTags       = 0;                          // Number of tags
String removeCommand  = "";                    // Remove command
String commands[]     = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};     // Commands for tags
String tags[]         = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};         // Tag IDs
String tagID          = "";                       // Current Tag ID
String prevTagID      = "";                       // Previous Tag ID

/**
 * @brief Writes a string to EEPROM starting at the specified address offset.
 *
 * This function writes the length of the string followed by the string's characters
 * to the EEPROM at the given address offset.
 *
 * @param addrOffset The starting address in EEPROM where the string will be written.
 * @param strToWrite The string to be written to EEPROM.
 */
void writeStringToEEPROM(int addrOffset, const String &strToWrite) {
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++) {
      EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
}

/**
 * @brief Reads a string from EEPROM starting at the specified address offset.
 *
 * This function reads the length of the string stored at the given EEPROM address offset,
 * then reads the subsequent characters to construct the string.
 *
 * @param addrOffset The starting address offset in EEPROM where the string is stored.
 * @return A String object containing the data read from EEPROM.
 */

String readStringFromEEPROM(int addrOffset) {
    int newStrLen = EEPROM.read(addrOffset);
    char data[newStrLen + 1];
    for (int i = 0; i < newStrLen; i++) {
        data[i] = EEPROM.read(addrOffset + 1 + i);
    }
    data[newStrLen] = '\0';
    return String(data);
}

/**
 * @brief Processes the given tag ID and executes the corresponding command if the tag is recognized.
 * 
 * This function compares the provided tag ID with a list of known tags. 
 * If a match is found, it Checks the mode of operation and sends the corresponding command to the Serial or Serial2 output.
 * If no match is found and debugging is enabled, it prints "UNKNOWN TAG" to the serial output.
 * 
 * @param tagID_ The tag ID to be processed.
 */
void processTagID(String tagID_){
  for (int i = 0; i < numTags; i++) {
    if (tagID_ == tags[i]) {
      if (!mode){ Serial.println(); Serial.println(commands[i]); }
      if (mode){ Serial2.println(); Serial2.println(commands[i]); }
      return;
    }
  }
  if (DEBUG) {Serial.println("UNKNOWN TAG");}
}

/**
 * @brief Reads an NFC tag using the PN532 NFC reader.
 * 
 * This function waits for an NTAG203 card and reads its UID. It handles three main scenarios:
 * 1. No change in card presence.
 * 2. A new card is detected.
 * 3. The card is removed.
 * 
 * When a new card is detected, the UID is stored in the `tagID` variable and processed.
 * If the card is removed, it checks if the removed card's UID matches any known tags and performs the necessary actions.
 */
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
      Serial.println("TAG ID: "+ tagID); Serial.print("  UID Value: "); nfc.PrintHex(uid, uidLength);
      Serial.println("");
    }
    processTagID(tagID);
    return;
  }
  // IF CARD REMOVED
  if (!success && cardPresesnt) {
    cardPresesnt = false;
    if(DEBUG) {Serial.println("CARD REMOVED");}
    for (int i = 0; i < numTags; i++) {
      if (prevTagID == tags[i]) {
        if (!mode){ Serial.println(); Serial.println(removeCommand); }
        if (mode){ Serial2.println(); Serial2.println(removeCommand); }
        return;
      }
    }
  }
}

/**
 * @brief Initializes the NFC module and checks for the PN53x board.
 * 
 * This function begins communication with the NFC module and retrieves the firmware version.
 * If the PN53x board is not found, it halts the program. If the board is found, it prints
 * the chip and firmware version information to the Serial monitor and indicates that it is
 * waiting for an ISO14443A card.
 */
void nfcInit(){
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  
  if (!versiondata) {
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

/**
 * @brief Processes the input data string and performs various actions based on its prefix.
 * 
 * This function handles different commands based on the prefix of the input data string:
 * - "N<num>": Sets the number of tags and stores it in EEPROM.
 * - "T<index>": Sets the last placed tag ID for the specified index and stores it in EEPROM.
 * - "C<index><command>": Sets a command for the specified index and stores it in EEPROM.
 * - "R<command>": Sets the tag remove command and stores it in EEPROM.
 * - "M<mode>": Sets the mode of operation (Master or Standalone) and stores it in EEPROM.
 * - "HELP": Prints help information about the available commands.
 * 
 * The function uses EEPROM to store and retrieve data, and communicates via Serial and Serial Bluetooth.
 * 
 * @param data The input data string containing the command and its parameters.
 */
void processData(String data) {
  if (data.startsWith("N")) {
    numTags = data.substring(1, data.length()).toInt();
    if (numTags > 20) numTags = 10;                   // Ensure numTags does not exceed array bounds
    Serial.println(numTags);
    EEPROM.write(0, numTags);
    EEPROM.commit();
    delay(10);
    int num = EEPROM.read(0);
    SerialBT.println("NUM TAGS SET TO: " + String(num));
    Serial.println("NUM TAGS SET TO: " + String(num));
    return;
  } else if (data.startsWith("T")) {
    int index = data.substring(1, data.length()).toInt() - 1;
    if (index >= 0 && index < 20) {
      tags[index] = prevTagID;
      writeStringToEEPROM(10 + index * 10, prevTagID);
      EEPROM.commit();
      delay(10);
    }
    for (int i = 0; i < numTags; i++) {
      String tagID_ = readStringFromEEPROM(10 + i * 10);
      SerialBT.println("Index: " + String(i+1) + " Tag ID: " + tagID_);
      Serial.println("Index: " + String(i+1) + " Tag ID: " + tagID_);
    } 
    return;
  } else if (data.startsWith("C")) {
    int index = data.substring(1, data.length()).toInt() - 1;
    if (index >= 0 && index < 20) {
      commands[index] = data.substring(3, data.length());
      writeStringToEEPROM(200 + index * 10, commands[index]);
      EEPROM.commit();
      delay(10);
    }
    for (int i = 0; i < numTags; i++) {
      String command = readStringFromEEPROM(200 + i * 10);
      SerialBT.println("Index: " + String(i+1) + " Command: " + command);
      Serial.println("Index: " + String(i+1) + " Command: " + command);
    }
    return;
  } else if (data.startsWith("R")) {
    removeCommand = data.substring(1, data.length());
    writeStringToEEPROM(400, removeCommand);
    EEPROM.commit();
    String command = readStringFromEEPROM(400);
    delay(10);
    SerialBT.println("Remove Command: " + command);
    Serial.println("Remove Command: " + command);
    return;
  } else if (data.indexOf("HELP")>=0){
    SerialBT.println("RFID Cube Podium PN532 - Firmware v1.0");
    SerialBT.println("N<num> - Set number of tags. 'Eg: N10' ");
    SerialBT.println("T<index> - Set Last placed tag ID for index. Eg: T1");
    SerialBT.println("C<index><command> - Set command for index. Eg: C1HELLO - Set HELLO command for index 1");
    SerialBT.println("R<command> - Set Tag Remove command. Eg: RREMOVED - Set REMOVED command for tag remove");

    Serial.println("RFID Cube Podium PN532 - Firmware v1.0"); Serial.println();
    Serial.println("N<num> - Set number of tags. 'Eg: N10' ");
    Serial.println("T<index> - Set Last placed tag ID for index. Eg: T1");
    Serial.println("C<index><command> - Set command for index. Eg: C1HELLO - Set HELLO command for index 1");
    Serial.println("R<command> - Set Tag Remove command. Eg: RREMOVED - Set REMOVED command for tag remove");
    return;
  }
}


/**
 * @brief Reads data from the serial input if available and processes it.
 *
 * This function checks if there is any data available on the serial input.
 * If data is available, it reads the incoming data as a string until a newline character is encountered.
 * The read data is then passed to the processData function for further processing.
 * If debugging is enabled, the incoming data is also printed to the serial output.
 */
void readSerial(){
  if (Serial.available()) {
    String incoming = Serial.readStringUntil('\n');
    processData(incoming);
    if (DEBUG) {Serial.println(incoming);}
  }
}

/**
 * @brief Reads data from the Bluetooth serial connection.
 *
 * This function checks if there is any data available on the Bluetooth serial connection.
 * If data is available, it reads the incoming data as a string until a newline character is encountered.
 * The incoming data is then processed by the processData function.
 * If debugging is enabled, the incoming data is also printed to the Bluetooth serial connection.
 */
void readBTSerial(){
  if (SerialBT.available()) {
    String incoming = SerialBT.readStringUntil('\n');
    processData(incoming);
    if (DEBUG) {SerialBT.println(incoming);}
  }
}

/**
 * @brief Initializes the EEPROM and reads stored data.
 *
 * This function initializes the EEPROM with a size of 512 bytes. 
 * It then reads the number of stored tags from the EEPROM at address 0. 
 * It also reads the mode of operation from address 5.
 * The remove command is read from address 300. For each tag, it reads the tag ID starting from address
 * 10 and increments by 10 for each subsequent tag. Similarly, it reads the commands
 * associated with each tag starting from address 100 and increments by 10 for each
 * subsequent command.
 */
void eepromInit(){
  EEPROM.begin(512);                                  // eeprom init
  numTags = EEPROM.read(0);                           // read number of tags
  if (numTags > 20) numTags = 10;                     // Ensure numTags does not exceed array bounds
  removeCommand = readStringFromEEPROM(400);          // read remove command
  for (int i = 0; i < numTags; i++) {
    tags[i] = readStringFromEEPROM(10 + i * 10);      // read tagIDs
    commands[i] = readStringFromEEPROM(200 + i * 10); // read commands
  }
}


/**
 * @brief Initializes the serial communication, Bluetooth communication, EEPROM, and NFC module.
 * 
 * This function sets up the necessary components for the system to function properly. It begins
 * by initializing the serial communication at a baud rate of 9600 for debugging purposes. 
 * Then Initiate the Serial2 communication at a baud rate of 115200 for Master mode communication.
 * Then, it starts the Bluetooth communication with the device name "RFID_PN532". 
 * After that, it initializes the EEPROM to store and retrieve data. 
 * Finally, it initializes the NFC module to enable NFC communication.
 */

void setup() {
  Serial.begin(9600);
  Serial2.begin(115200);
  SerialBT.begin("RFID_PN532");
  eepromInit();
  nfcInit();
}

/**
 * @brief Main loop function that continuously reads data from NFC, Bluetooth Serial, and Serial interfaces.
 * 
 * This function is called repeatedly in the main program loop. It performs the following tasks:
 * - Reads data from an NFC reader by calling the readNFC() function.
 * - Reads data from a Bluetooth Serial interface by calling the readBTSerial() function.
 * - Reads data from a standard Serial interface by calling the readSerial() function.
 */
void loop() {
  readNFC();
  readBTSerial();
  readSerial();
}