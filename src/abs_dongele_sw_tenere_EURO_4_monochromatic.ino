#include <SPI.h>
#include <mcp_can.h>
#include <EEPROM.h>

#define CAN_INT_PIN 2
#define CAN_CS_PIN 10

MCP_CAN CAN(CAN_CS_PIN);

const unsigned long canId = 0x2A0;
const unsigned long absId = 0x268;    // ABS ECU ID
const unsigned long engineId = 0x20A; // ECU ECU ID

const unsigned long absInit = 0x18;  // Initialization byte
const int eepromAddress = 0;

const byte absOnMsg[6] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
const byte absOffMsg[6] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x1C};
const byte buttonPressMsg[1] = {0x80}; // Button press message

const byte killSwitchByte = 0x02; // When killing the bike with the kill switch
const byte starterSwitchByte = 0x80; // When starting the bike over the started switch

const float delayMs = 4;

enum AbsState { ABS_ON, ABS_OFF };
AbsState currentState = ABS_ON;

enum EngineState { ENGINE_ON, ENGINE_OFF };
EngineState currentEngineState = ENGINE_OFF;

byte lastState[6];    // Holds the last state read from EEPROM

void sendAbsCanMessage(const byte* dataToSend, size_t length) {
    Serial.println("Sending CAN message:");
    for (byte i = 0; i < length; i++) {
        Serial.print(dataToSend[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    for (int i = 0; i < 500; i++) {
        if (CAN.sendMsgBuf(canId, 0, length, dataToSend) == CAN_OK) {
            Serial.print("Message sent [");
            Serial.print(i + 1);
            Serial.println("/500]");
        } else {
            Serial.println("Error sending message...");
        }
        delay(delayMs);  // Short delay to prevent flooding
    }
}

void processAbsStateChange(AbsState state) {
    switch (state) {
        case ABS_ON:
            Serial.println("ABS ON by default. No state change required.");
            break;
        case ABS_OFF:
            break;
    }
}

void restoreLastSavedState() {
    // Read saved data from EEPROM
    for (int i = 0; i < 6; i++) {
        lastState[i] = EEPROM.read(eepromAddress + i);
    }

    Serial.println("Restored saved ABS state:");
    for (byte i : lastState) {
        Serial.print(i, HEX);
        Serial.print(" ");
    }
    Serial.println();

    if (lastState[5] == 0x1C) { // ABS OFF
        currentState = ABS_OFF;
        Serial.println("Restored state: ABS OFF");
        sendAbsCanMessage(buttonPressMsg, 1);           // Send button press message
    } else { // Default to ABS ON
        currentState = ABS_ON;
        Serial.println("Restored state: ABS ON");
    }

    processAbsStateChange(currentState);
}

void updateEepromIfChanged(const byte* newData, size_t length) {
    bool dataChanged = false;
    for (size_t i = 0; i < length; i++) {
        if (EEPROM.read(eepromAddress + i) != newData[i]) {
            EEPROM.update(eepromAddress + i, newData[i]);
            dataChanged = true;
        }
    }
    if (dataChanged) {
        Serial.println("EEPROM updated.");
    }
}

void setup() {
    Serial.begin(115200);

    if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
        Serial.println("MCP2515 Initialized Successfully!");
    } else {
        Serial.println("Error Initializing MCP2515...");
        while (1); // Halt execution
    }

    CAN.setMode(MCP_NORMAL);

    Serial.println("Waiting for ABS initialization...");
    while (true) {
        if (!digitalRead(CAN_INT_PIN)) {
            unsigned long rxId;
            byte len = 0;
            byte rxBuf[8];

            if (CAN.readMsgBuf(&rxId, &len, rxBuf) == CAN_OK && rxId == absId && rxBuf[5] == absInit) {
                Serial.println("Received initialization message. Restoring state...");
                delay(1000);
                restoreLastSavedState();
                break;
            }
        }
    }

    Serial.println("Setup complete. Listening for CAN messages...");
}

void loop() {
    if (!digitalRead(CAN_INT_PIN)) {
        unsigned long rxId;
        byte len = 0;
        byte rxBuf[8];

        if (CAN.readMsgBuf(&rxId, &len, rxBuf) == CAN_OK) {
            if (rxId == absId && currentEngineState == ENGINE_ON) {
                byte requiredByte = rxBuf[5];
                byte currentByte = lastState[5];

                if (requiredByte == absInit) {
                    Serial.println("Received ABS initialization. Sending saved state...");
                    restoreLastSavedState();
                } else if (requiredByte != currentByte && requiredByte != 0x1A) {
                    Serial.println("State change detected. Updating EEPROM...");
                    updateEepromIfChanged(rxBuf, len);
                    memcpy(lastState, rxBuf, len);

                    currentState = (currentByte == 0x1C) ? ABS_OFF : ABS_ON;
                    processAbsStateChange(currentState);
                } else if (requiredByte == 0x1A) {
                    Serial.println("Received 0x1A. Sending last saved ABS state...");
                    processAbsStateChange((AbsState)currentByte);
                }
            } else if (rxId == engineId) {
                byte engineButtonMsg = rxBuf[7];
                if (engineButtonMsg == killSwitchByte) {
                    currentEngineState = ENGINE_OFF;
                    Serial.println("Kill switch pressed. Setting engine state to OFF.");
                } else if (engineButtonMsg == starterSwitchByte && currentEngineState == ENGINE_OFF) {
                    currentEngineState = ENGINE_ON;
                    Serial.println("Starter switch pressed or engine state already OFF. Setting engine state to ON.");
                    currentState = (lastState[5] == 0x1C) ? ABS_OFF : ABS_ON;
                    processAbsStateChange(currentState);
                }
            }
        }
    }
}
