#include <SPI.h>
#include <mcp_can.h>
#include <EEPROM.h>

#define CAN_INT_PIN 2
#define CAN_CS_PIN 10

MCP_CAN CAN(CAN_CS_PIN);

const unsigned long canId = 0x226;
const unsigned long absId = 0x268;    // ABS ECU ID
const unsigned long absButton = 0x2A0;

const int eepromAddress = 0;
const unsigned long messageInterval = 4;
const unsigned long buttonMessageTimeoutMs = 150;
const byte dataLength = 7;
const byte savedDataLength = 6;

const byte absOnMsg[dataLength] PROGMEM = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const byte rearAbsOffMsg[dataLength] PROGMEM = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};
const byte absOffMsg[dataLength] PROGMEM = {0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00};

enum AbsState { ABS_ON, ABS_OFF, REAR_ABS_OFF };
AbsState currentState = ABS_ON;

enum AbsStatusAction {
    ABS_STATUS_IGNORED,
    ABS_RESTORE_CONFIRMED,
    ABS_DESIRED_STATE_CHANGED
};

struct AbsRestoreState {
    byte desiredState;
    bool restorePending;
};

bool isStableAbsState(byte state) {
    return state == 0x01 || state == 0x1B || state == 0x1D;
}

void beginAbsRestore(AbsRestoreState& state, byte desiredState) {
    if (!isStableAbsState(desiredState)) {
        return;
    }
    state.desiredState = desiredState;
    state.restorePending = true;
}

AbsStatusAction observeAbsStatus(AbsRestoreState& state, byte observedState) {
    if (!isStableAbsState(observedState)) {
        return ABS_STATUS_IGNORED;
    }

    if (state.restorePending) {
        if (observedState == state.desiredState) {
            state.restorePending = false;
            return ABS_RESTORE_CONFIRMED;
        }
        return ABS_STATUS_IGNORED;
    }

    if (observedState != state.desiredState) {
        state.desiredState = observedState;
        return ABS_DESIRED_STATE_CHANGED;
    }

    return ABS_STATUS_IGNORED;
}

byte lastState[savedDataLength];
byte canMsg[dataLength];
AbsRestoreState restoreState = {0x01, true};

unsigned long buttonLastReceived = 0; // Timestamp of last button message
int buttonMsgCounter = 0;
bool buttonMsgSent = false;

byte readSavedData(byte* savedData) {
    for (int i = 0; i < savedDataLength; i++) {
        savedData[i] = EEPROM.read(eepromAddress + i);
    }
    return savedData[5];
}

bool hasAbsStateByte(byte length) {
    return length >= savedDataLength;
}

void resetButtonSequence() {
    buttonMsgCounter = 0;
    buttonMsgSent = false;
}

void expireButtonSequenceIfNeeded() {
    if (buttonMsgCounter > 0 && millis() - buttonLastReceived > buttonMessageTimeoutMs) {
        resetButtonSequence();
    }
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

void saveDesiredAbsState(byte desiredState) {
    if (!isStableAbsState(desiredState)) {
        return;
    }

    beginAbsRestore(restoreState, desiredState);
    lastState[savedDataLength - 1] = desiredState;
    EEPROM.update(eepromAddress + savedDataLength - 1, desiredState);
}

void handleObservedAbsState(const byte* observedData) {
    byte observedState = observedData[savedDataLength - 1];
    AbsStatusAction action = observeAbsStatus(restoreState, observedState);

    if (action == ABS_RESTORE_CONFIRMED) {
        Serial.println("Restored ABS state confirmed by ECU.");
    } else if (action == ABS_DESIRED_STATE_CHANGED) {
        Serial.println("Stable ABS state change detected. Updating EEPROM...");
        updateEepromIfChanged(observedData, sizeof(lastState));
        memcpy(lastState, observedData, sizeof(lastState));
        Serial.println(observedState, HEX);
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
      if (CAN.sendMsgBuf(canId, 0, length, dataToSend) == CAN_OK) {
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
            memcpy_P(canMsg, absOnMsg, dataLength);
            break;
        case REAR_ABS_OFF:
            memcpy_P(canMsg, rearAbsOffMsg, dataLength);
            break;
        case ABS_OFF:
            memcpy_P(canMsg, absOffMsg, dataLength);
            break;
    }
    sendAbsCanMessage(canMsg, dataLength);
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
    beginAbsRestore(restoreState, savedState);
    processAbsStateChange(currentState);
    Serial.println("Restored last saved ABS state.");
}

void setup() {
    Serial.begin(115200);
    pinMode(CAN_INT_PIN, INPUT);

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
    expireButtonSequenceIfNeeded();

    if (!digitalRead(CAN_INT_PIN)) {
        unsigned long rxId;
        byte len = 0;
        byte rxBuf[8];

        if (CAN.readMsgBuf(&rxId, &len, rxBuf) == CAN_OK) {
            if (rxId == absId && hasAbsStateByte(len)) {
                byte requiredByte = rxBuf[5];

                // Check if the received value is 0x11 (binary 0100011)
                if (requiredByte == 0x11) {
                    Serial.println("Received 0100011 (0x11), sending last saved ABS state:");
                    delay(3000); // Sleep for 3 seconds before sending
                    restoreLastSavedState();
                } else if (requiredByte == 0x1A){
                  delay(1000);
                  restoreLastSavedState();
                } else {
                  handleObservedAbsState(rxBuf);
                }
            } else if (rxId == absButton && len >= 1) {
                buttonLastReceived = millis();
                if (rxBuf[0] == 0x80) {
                  buttonMsgCounter++;
                  if (buttonMsgCounter > 50 && !buttonMsgSent) {
                    saveDesiredAbsState(0x1D);
                    processAbsStateChange(ABS_OFF);
                    buttonMsgSent = true;
                  }
                } else if (rxBuf[0] == 0x00) {
                  if (buttonMsgCounter > 0 && buttonMsgCounter <= 50 && !buttonMsgSent && lastState[5] != 0x00) {
                    saveDesiredAbsState(0x1B);
                    processAbsStateChange(REAR_ABS_OFF);
                    buttonMsgSent = true;
                  }
                  resetButtonSequence();
                }
            }
        }
    }
}
