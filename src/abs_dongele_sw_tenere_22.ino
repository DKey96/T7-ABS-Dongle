#include <SPI.h>
#include <mcp_can.h>
#include <EEPROM.h>

#define CAN_INT_PIN 2
#define CAN_CS_PIN 10

MCP_CAN CAN(CAN_CS_PIN);

const unsigned long canId = 0x226;
const unsigned long absId = 0x268;    // ABS ECU ID
const unsigned long absInit = 0x18;  // Initialization byte
const int eepromAddress = 0;

const byte absOnMsg[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const byte absOffMsg[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x1C};

enum AbsState { ABS_ON, ABS_OFF };
AbsState currentState = ABS_ON;

byte lastState[6];    // Holds the last state read from EEPROM
unsigned long previousMillis = 0;
const unsigned long messageTimeout = 1000; // 1-second timeout

void sendAbsCanMessage(const byte* dataToSend, size_t length) {
    Serial.println("Sending ABS message:");
    for (byte i = 0; i < length; i++) {
        Serial.print(dataToSend[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    for (int i = 0; i < 25; i++) {
        if (CAN.sendMsgBuf(canId, 0, length, dataToSend) == CAN_OK) {
            Serial.print("Message sent [");
            Serial.print(i + 1);
            Serial.println("/25]");
        } else {
            Serial.println("Error sending message...");
        }
        delay(1);  // Short delay to prevent flooding
    }
}

void processAbsStateChange(AbsState state) {
    switch (state) {
        case ABS_ON:
            Serial.println("ABS ON by default. No state change required.");
            break;
        case ABS_OFF:
            sendAbsCanMessage(absOffMsg, sizeof(absOffMsg));
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
        sendAbsCanMessage(absOffMsg, sizeof(absOffMsg)); // Send ABS OFF message
        byte buttonPressMsg[1] = {0x80}; // Button press message
        sendAbsCanMessage(buttonPressMsg, 1); // Send button press
    } else { // Default to ABS ON
        currentState = ABS_ON;
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
            if (rxId == absId) {
                byte requiredByte = rxBuf[5];

                if (requiredByte == absInit) {
                    Serial.println("Received ABS initialization. Sending saved state...");
                    restoreLastSavedState();
                } else if (requiredByte != lastState[5]) {
                    Serial.println("State change detected. Updating EEPROM...");
                    updateEepromIfChanged(rxBuf, len);
                    memcpy(lastState, rxBuf, len);

                    currentState = (lastState[5] == 0x1C) ? ABS_OFF : ABS_ON;
                    processAbsStateChange(currentState);
                }
            }
        }
    }
}
