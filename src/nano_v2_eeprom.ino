#include <SPI.h>
#include <mcp_can.h>
#include <EEPROM.h>

#define CAN_INT_PIN 2  // MCP2515 INT pin connected to Arduino pin 2
#define CAN_CS_PIN 10  // MCP2515 CS pin connected to Arduino pin 10

MCP_CAN CAN(CAN_CS_PIN);  // Set CS pin for MCP2515

// CAN message parameters
const unsigned long canId = 0x226;        // CAN ID in hexadecimal
const byte dataLength = 7;                // Data Length Code (DLC) - 6 bytes
const unsigned long messageInterval = 8;  // Message interval in milliseconds

const byte absOnStationary[6] = { 0x00, 0x01, 0x00, 0x00, 0x00, 0x01 };
const byte absOnDriving[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
const byte readAbsOnStationary[6] = { 0x00, 0x01, 0x00, 0x00, 0x00, 0x1B };
const byte readAbsOnDriving[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x1B };
const byte absOffStationary[6] = { 0x00, 0x01, 0x00, 0x00, 0x00, 0x1D };
const byte absOffDriving[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x1D };

// CAN messages
const byte absOnMsg[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const byte rearAbsOffMsg[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
const byte absOffMsg[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };

byte canMsg[6];

// PID 268 that we are interested in reading and saving
const unsigned long absId = 0x268;
const unsigned long absButton = 0x2A0;

// counter for button press (roughly 50 msgs == short press, more == long press)
int buttonMsgCounter = 0;
bool buttonMsgSent = false;

// Predefined message to wait for
const byte absInitMsg[6] = { 0x00, 0x01, 0x00, 0x00, 0x00, 0x11 };

// EEPROM address to store the state
const int eepromAddress = 0;

// Variable to store the last known state
byte lastState[6];  // Assuming 8 bytes max for CAN message data

// Helper function to compare saved data with predefined patterns
byte checkSavedData(const byte savedData[6]) {
  byte result = 0xFF;  // Return 0xFF if no match is found

  if (savedData[5] == 0x01) {
    result = 0x00;
  } else if (savedData[5] == 0x1B) {
    result = 0x01;
  } else if (savedData[5] == 0x1D) {
    result = 0x02;
  }
  return result;
}

void sendAbsCanMessage(byte dataToSend[6]) {
  Serial.println("Sending ABS message:");
  for (byte i = 0; i < 6; i++) {
    Serial.print(dataToSend[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // Send the data
  for (int i = 0; i < 25; i++) {
    if (CAN.sendMsgBuf(canId, 0, dataLength, dataToSend) == CAN_OK) {
      Serial.print("Initial message sent [");
      Serial.print(i + 1);
      Serial.println("/25] based on saved state");
    } else {
      Serial.println("Error sending initial message...");
    }
    delay(messageInterval);  // 10 ms delay between messages
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize CAN controller
  if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("MCP2515 Initialized Successfully!");
  } else {
    Serial.println("Error Initializing MCP2515...");
    while (1)
      ;  // Halt if initialization fails
  }

  CAN.setMode(MCP_NORMAL);  // Set MCP2515 to normal mode

  // Wait if the message from 0x268 is 00 01 00 00 00 11
  while (true) {
    unsigned long rxId;
    byte len = 0;
    byte rxBuf[8];

    if (!digitalRead(CAN_INT_PIN) && CAN.readMsgBuf(&rxId, &len, rxBuf) == CAN_OK) {
      if (rxId == absId && memcmp(rxBuf, absInitMsg, 6) == 0) {
        Serial.println("Received message 00 01 00 00 00 11 from 0x268, waiting...");
        delay(1000);  // Wait for 1 second before checking again
      } else {
        // Exit the loop if the condition is no longer met
        break;
      }
    }
  }

  // Read saved data from EEPROM
  byte savedData[6];
  for (int i = 0; i < 6; i++) {
    savedData[i] = EEPROM.read(eepromAddress + i);  // Adjust addresses accordingly
    Serial.print(savedData[i], HEX);
    Serial.print(" ");
  }
  Serial.println("");

  // Initialize the last state from savedData
  for (int i = 0; i < sizeof(savedData); i++) {
    lastState[i] = savedData[i];
  }

  // Check saved data and set initData[5]
  byte result = checkSavedData(savedData);
  if (result != 0xFF) {
    Serial.print("Matched condition: ");
    for (int i = 0; i < 6; i++) {
      Serial.print(savedData[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    switch (result) {
      case 0x00:
        memcpy(canMsg, absOnMsg, 6);
        break;
      case 0x01:
        memcpy(canMsg, rearAbsOffMsg, 6);
        break;
      case 0x02:
        memcpy(canMsg, absOffMsg, 6);
        break;
    }
  }
  Serial.print("Printing result: ");
  Serial.println(result, HEX);

  // Send the determined initial data 25 times with 10 ms interval
  if (
    memcmp(savedData, absOnStationary, 6) != 0 && memcmp(savedData, absOnDriving, 6) != 0 && (result != 0x00 || result != 0xFF)) {
    sendAbsCanMessage(canMsg);
  }
  Serial.println("Listening for ID 0x268...");
}

void loop() {
  // Check for received CAN messages
  if (!digitalRead(CAN_INT_PIN)) {
    unsigned long rxId;
    byte len = 0;
    byte rxBuf[8];

    if (CAN.readMsgBuf(&rxId, &len, rxBuf) == CAN_OK) {
      // Check if the received message is for PID 268, but only if the last byte changed
      if (rxId == absId && rxBuf[5] != lastState[5]) {
        // Compare the received data with the last stored state
        bool stateChanged = false;
        for (int i = 0; i < len; i++) {
          if (rxBuf[i] != lastState[i]) {
            stateChanged = true;
            break;
          }
        }

        // If state has changed, update EEPROM and lastState
        if (stateChanged) {
          Serial.println("State has changed, updating EEPROM... Data:");
          for (int i = 0; i < len; i++) {
            EEPROM.write(eepromAddress + i, rxBuf[i]);
            lastState[i] = rxBuf[i];
            Serial.print(rxBuf[i], HEX);
          }
          Serial.println("");
          delay(10);
        }
      } else if (rxId == absButton && rxBuf[0] == 0x80) {
        buttonMsgCounter++;
        if (35 < buttonMsgCounter && buttonMsgSent == false) {
          sendAbsCanMessage(absOffMsg);
          Serial.println(buttonMsgCounter);
          delay(1000);
          buttonMsgSent = true;
        }
      } else if (rxId == absButton && rxBuf[0] == 0x00) {
        if (buttonMsgCounter > 0 && buttonMsgCounter <= 20 && buttonMsgSent == false && lastState[5] == 0x01 && lastState[5] != 0x1D) {
          sendAbsCanMessage(rearAbsOffMsg);
          Serial.println(buttonMsgCounter);
          delay(1000);
          buttonMsgSent = true;
        }
        buttonMsgCounter = 0;
        buttonMsgSent = false;
      }
    }
  }
}
