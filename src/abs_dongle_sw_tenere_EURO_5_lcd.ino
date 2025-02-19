#include <SPI.h>
#include <mcp_can.h>
#include <EEPROM.h>

#define CAN_INT_PIN 2
#define CAN_CS_PIN 10

MCP_CAN CAN(CAN_CS_PIN);

const unsigned long canId = 0x226;
const unsigned long absId = 0x268;    // ABS ECU ID
const unsigned long absButton = 0x2A0;

const byte absOnMsg[6] PROGMEM = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const byte rearAbsOffMsg[6] PROGMEM = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
const byte absOffMsg[6] PROGMEM = {0x00, 0x00, 0x00, 0x00, 0x00, 0x02};

const int eepromAddress = 0;
const unsigned long messageInterval = 4;
const byte dataLength = 7;

enum AbsState { ABS_ON, ABS_OFF, REAR_ABS_OFF };
AbsState currentState = ABS_ON;

byte lastState[6];
byte canMsg[6];

unsigned long buttonLastReceived = 0; // Timestamp of last button message
int buttonMsgCounter = 0;
bool buttonMsgSent = false;

byte readSavedData(byte* savedData) {
    for (int i = 0; i < 6; i++) {
        savedData[i] = EEPROM.read(eepromAddress + i);
    }
    return savedData[5];
}

void updateEepromIfChanged(const byte* newData, size_t length) {
    bool dataChanged = false;
    for (size_t i = 0; i < length; i++) {
        if (EEPROM.read(eepromAddress + i) != newData[i]) {
            EEPROM.update(eepromAddress + i, newData[i]);
            dataChanged = true;
        }
    }
    if (dataChanged == true) {
      Serial.println("EEPROM updated");
    }
}

void sendAbsCanMessage(const byte* dataToSend, size_t length) {
    Serial.println("Sending ABS message:");
    for (size_t i = 0; i < length; i++) {
        Serial.print(dataToSend[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    bool success = false;
    for (int i = 0; i < 25; i++) {
      if (CAN.sendMsgBuf(canId, 0, dataLength, dataToSend) == CAN_OK) {
        success = true;
      }
      delay(messageInterval);
    }
    if (success == true){
      Serial.println("Message sent...");
    } else {
      Serial.println("Error sending message...");
    }
}

void processAbsStateChange(AbsState state) {
    switch (state) {
        case ABS_ON:
            memcpy_P(canMsg, absOnMsg, 6);
            break;
        case REAR_ABS_OFF:
            memcpy_P(canMsg, rearAbsOffMsg, 6);
            break;
        case ABS_OFF:
            memcpy_P(canMsg, absOffMsg, 6);
            break;
    }
    sendAbsCanMessage(canMsg, 6);
}

void restoreLastSavedState() {
    byte savedState = readSavedData(lastState);
    Serial.println("Saved state:");
    Serial.println(savedState, HEX);
    switch (savedState) {
        case 0x01: currentState = ABS_ON; break;
        case 0x1B: currentState = REAR_ABS_OFF; break;
        case 0x1D: currentState = ABS_OFF; break;
        default: return; // Invalid state, do nothing
    }
    processAbsStateChange(currentState);
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
    restoreLastSavedState();
}

void loop() {
    if (!digitalRead(CAN_INT_PIN)) {
        unsigned long rxId;
        byte len = 0;
        byte rxBuf[8];

        if (CAN.readMsgBuf(&rxId, &len, rxBuf) == CAN_OK) {
            if (rxId == absId) {
                byte requiredByte = rxBuf[5];
                byte currentByte = lastState[5];

                // Check if the received value is 0x11 (binary 0100011)
                if (requiredByte == 0x11) {
                    Serial.println("Received 0100011 (0x11), sending last saved ABS state:");
                    delay(3000); // Sleep for 3 seconds before sending
                    restoreLastSavedState();
                } else if (requiredByte != currentByte && requiredByte != 0x1A) {
                    Serial.println("State has changed, updating EEPROM...");
                    updateEepromIfChanged(rxBuf, dataLength);
                    Serial.println(requiredByte, HEX);
                    lastState[5] = requiredByte;
                } else if (requiredByte == 0x1A){
                  delay(1000);
                  processAbsStateChange((AbsState)currentByte);
                }
            } else if (rxId == absButton) {
                if (rxBuf[0] == 0x80) {
                  buttonMsgCounter++;
                  if (buttonMsgCounter > 50 && !buttonMsgSent) {
                    processAbsStateChange(ABS_OFF);
                    buttonMsgSent = true;
                  }
                } else if (rxBuf[0] == 0x00) {
                  if (buttonMsgCounter > 0 && buttonMsgCounter <= 50 && !buttonMsgSent && lastState[5] != 0x00) {
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
