#include <SPI.h>
#include <mcp_can.h>
#include <EEPROM.h>

#define CAN_INT_PIN 2
#define CAN_CS_PIN 10

MCP_CAN CAN(CAN_CS_PIN);

const unsigned long canId = 0x226;
const byte dataLength = 7;
const unsigned long messageInterval = 8;

const byte absOnStationary[6] PROGMEM = { 0x00, 0x01, 0x00, 0x00, 0x00, 0x01 };
const byte absOnDriving[6] PROGMEM = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
const byte absOffStationary[6] PROGMEM = { 0x00, 0x01, 0x00, 0x00, 0x00, 0x1D };
const byte absOffDriving[6] PROGMEM = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x1D };

const byte absOnMsg[6] PROGMEM = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const byte rearAbsOffMsg[6] PROGMEM = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
const byte absOffMsg[6] PROGMEM = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };

const unsigned long absId = 0x268;
const unsigned long absButton = 0x2A0;

int buttonMsgCounter = 0;
bool buttonMsgSent = false;

const byte absInitMsg[6] PROGMEM = { 0x00, 0x01, 0x00, 0x00, 0x00, 0x11 };
const int eepromAddress = 0;

byte lastState[6];
byte canMsg[6];

byte checkSavedData(byte savedData[6]) {
  switch (savedData[5]) {
    case 0x01: return 0x00;
    case 0x1B: return 0x01;
    case 0x1D: return 0x02;
    default: return 0xFF;
  }
}

void sendAbsCanMessage(const byte* dataToSend) {
  Serial.println("Sending ABS message:");
  for (byte i = 0; i < 6; i++) {
    Serial.print(dataToSend[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  for (int i = 0; i < 25; i++) {
    if (CAN.sendMsgBuf(canId, 0, dataLength, dataToSend) == CAN_OK) {
      Serial.print("Message sent [");
      Serial.print(i + 1);
      Serial.println("/25]");
    } else {
      Serial.println("Error sending message...");
    }
    delay(messageInterval);
  }
}

void setup() {
  Serial.begin(115200);

  if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("MCP2515 Initialized Successfully!");
  } else {
    Serial.println("Error Initializing MCP2515...");
    while (1);
  }

  CAN.setMode(MCP_NORMAL);

  while (true) {
    unsigned long rxId;
    byte len = 0;
    byte rxBuf[8];

    if (!digitalRead(CAN_INT_PIN) && CAN.readMsgBuf(&rxId, &len, rxBuf) == CAN_OK) {
      if (rxId == absId && memcmp(rxBuf, absInitMsg, 6) == 0) {
        Serial.println("Received initialization message, waiting...");
        delay(1000);
      } else {
        break;
      }
    }
  }

  byte savedData[6];
  for (int i = 0; i < 6; i++) {
    savedData[i] = EEPROM.read(eepromAddress + i);
    Serial.print(savedData[i], HEX);
    Serial.print(" ");
  }
  Serial.println("");

  memcpy(lastState, savedData, 6);
  byte result = checkSavedData(savedData);

  if (result != 0xFF) {
    Serial.print("Matched condition: ");
    for (int i = 0; i < 6; i++) {
      Serial.print(savedData[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    switch (result) {
      case 0x00: memcpy_P(canMsg, absOnMsg, 6); break;
      case 0x01: memcpy_P(canMsg, rearAbsOffMsg, 6); break;
      case 0x02: memcpy_P(canMsg, absOffMsg, 6); break;
    }

    sendAbsCanMessage(canMsg);
  }

  Serial.println("Listening for ID 0x268...");
}

void loop() {
  if (!digitalRead(CAN_INT_PIN)) {
    unsigned long rxId;
    byte len = 0;
    byte rxBuf[8];

    if (CAN.readMsgBuf(&rxId, &len, rxBuf) == CAN_OK) {
      if (rxId == absId && rxBuf[5] != lastState[5]) {
        if (memcmp(rxBuf, lastState, len) != 0) {
          Serial.println("State has changed, updating EEPROM... Data:");
          for (int i = 0; i < len; i++) {
            EEPROM.update(eepromAddress + i, rxBuf[i]);
            lastState[i] = rxBuf[i];
            Serial.print(rxBuf[i], HEX);
          }
          Serial.println("");
        }
      } else if (rxId == absButton) {
        if (rxBuf[0] == 0x80) {
          buttonMsgCounter++;
          if (buttonMsgCounter > 35 && !buttonMsgSent) {
            sendAbsCanMessage(absOffMsg);
            Serial.println(buttonMsgCounter);
            buttonMsgSent = true;
          }
        } else if (rxBuf[0] == 0x00) {
          if (buttonMsgCounter > 0 && buttonMsgCounter <= 20 && !buttonMsgSent && lastState[5] == 0x01 && lastState[5] != 0x1D) {
            sendAbsCanMessage(rearAbsOffMsg);
            Serial.println(buttonMsgCounter);
            buttonMsgSent = true;
          }
          buttonMsgCounter = 0;
          buttonMsgSent = false;
        }
      }
    }
  }
}
