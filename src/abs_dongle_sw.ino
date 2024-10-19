#include <SPI.h>
#include <mcp_can.h>
#include <EEPROM.h>

#define CAN_INT_PIN 2
#define CAN_CS_PIN 10

MCP_CAN CAN(CAN_CS_PIN);

const unsigned long canId = 0x226;
const byte dataLength = 7;
const unsigned long messageInterval = 1;

const byte absOnMsg[6] PROGMEM = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const byte rearAbsOffMsg[6] PROGMEM = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
const byte absOffMsg[6] PROGMEM = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };

const unsigned long absId = 0x268;  // ABS ECU ID
const unsigned long absButton = 0x2A0;

int buttonMsgCounter = 0;
bool buttonMsgSent = false;
const int eepromAddress = 0;

byte lastState[6];
byte canMsg[6];

enum AbsState { ABS_ON, ABS_OFF, REAR_ABS_OFF };
AbsState currentState = ABS_ON;

unsigned long previousMillis = 0;
const unsigned long messageTimeout = 1000;  // Timeout for 1 second (1000 ms)

byte checkSavedData(byte savedData[6]) {
    switch (savedData[5]) {
        case 0x01: return 0x00; // ABS ON
        case 0x1B: return 0x01; // REAR ABS ON
        case 0x1D: return 0x02; // ABS OFF
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

void processAbsStateChange(AbsState state) {
    switch (state) {
        case ABS_ON:
            memcpy_P(canMsg, absOnMsg, 6);
            sendAbsCanMessage(canMsg);
            break;
        case REAR_ABS_OFF:
            memcpy_P(canMsg, rearAbsOffMsg, 6);
            sendAbsCanMessage(canMsg);
            break;
        case ABS_OFF:
            memcpy_P(canMsg, absOffMsg, 6);
            sendAbsCanMessage(canMsg);
            break;
    }
}

void restoreLastSavedState() {
    // Read saved data from EEPROM
    byte savedData[6];
    for (int i = 0; i < 6; i++) {
        savedData[i] = EEPROM.read(eepromAddress + i);
    }

    byte result = checkSavedData(savedData);

    if (result != 0xFF) {
        switch (result) {
            case 0x00: currentState = ABS_ON; break;
            case 0x01: currentState = REAR_ABS_OFF; break;
            case 0x02: currentState = ABS_OFF; break;
        }
        processAbsStateChange(currentState);
    }

    Serial.println("Restored last saved ABS state.");
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
            byte requiredByte = rxBuf[5];
            if (rxId == absId && requiredByte == 0x11) {
                Serial.println("Received initialization message, waiting...");
                delay(1000);
            } else {
                break;
            }
        }
    }

    // Read saved data from EEPROM
    byte savedData[6];
    for (int i = 0; i < 6; i++) {
        savedData[i] = EEPROM.read(eepromAddress + i);
        Serial.print(savedData[i], HEX);
        Serial.print(" ");
    }
    Serial.println("");

    // Load the last known state
    memcpy(lastState, savedData, 6);
    byte result = checkSavedData(savedData);

    if (result != 0xFF) {
        switch (result) {
            case 0x00: currentState = ABS_ON; break;
            case 0x01: currentState = REAR_ABS_OFF; break;
            case 0x02: currentState = ABS_OFF; break;
        }
        processAbsStateChange(currentState);
    }

    Serial.println("Listening for ID 0x268...");
}

void loop() {
    if (!digitalRead(CAN_INT_PIN)) {
        unsigned long rxId;
        byte len = 0;
        byte rxBuf[8];

        if (CAN.readMsgBuf(&rxId, &len, rxBuf) == CAN_OK) {
            if (rxId == absId) {
                byte requiredByte = rxBuf[5];
                byte currentState = lastState[5];

                // Check if the received value is absInit
                if (requiredByte == 0x11) {
                    Serial.println("Received 0100011 (0x11), sending last saved ABS state:");
                    delay(1000); // Sleep for 3 seconds before sending
                    restoreLastSavedState();

                } else if (requiredByte != currentState) {
                    // Update the last state and EEPROM if necessary
                    Serial.println("State has changed, updating EEPROM... Data:");
                    bool dataChanged = false;
                    for (int i = 0; i < len; i++) {
                        if (EEPROM.read(eepromAddress + i) != rxBuf[i]) {
                            EEPROM.update(eepromAddress + i, rxBuf[i]);
                            dataChanged = true;
                        }
                        lastState[i] = rxBuf[i];
                        Serial.print(rxBuf[i], HEX);
                    }
                    Serial.println("");
                    if (dataChanged) {
                        Serial.println("EEPROM updated.");
                    }
                }
            } else if (rxId == absButton) {
                if (rxBuf[0] == 0x80) {
                  buttonMsgCounter++;
                  if (buttonMsgCounter > 35 && !buttonMsgSent) {
                    processAbsStateChange(ABS_OFF);
                    buttonMsgSent = true;
                  }
                } else if (rxBuf[0] == 0x00) {
                  if (buttonMsgCounter > 0 && buttonMsgCounter <= 20 && !buttonMsgSent && lastState[5] == 0x01 && lastState[5] != 0x1D) {
                    processAbsStateChange(REAR_ABS_OFF);
                    buttonMsgSent = true;
                  }
                  buttonMsgCounter = 0;
                  buttonMsgSent = false;
                }
            }
        }
    }
}
