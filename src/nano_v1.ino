#include <SPI.h>
#include <mcp_can.h>

#define CAN_INT_PIN 2  // MCP2515 INT pin connected to Arduino pin 2
#define CAN_CS_PIN 10  // MCP2515 CS pin connected to Arduino pin 10

MCP_CAN CAN(CAN_CS_PIN);  // Set CS pin for MCP2515

// CAN message parameters
const unsigned long canId = 0x226;  // CAN ID in hexadecimal
const byte dataLength = 7;         // Data Length Code (DLC) - 7 bytes
const byte messageData[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01}; // Data payload
const unsigned long messageInterval = 10;  // Message interval in milliseconds

void setup() {
  Serial.begin(115200);
  
  // Initialize CAN controller
  if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("MCP2515 Initialized Successfully!");
  } else {
    Serial.println("Error Initializing MCP2515...");
    while (1); // Halt if initialization fails
  }

  CAN.setMode(MCP_NORMAL);  // Set MCP2515 to normal mode
  for (int i = 0; i < 25; i++) {
    byte sendStatus = CAN.sendMsgBuf(canId, 0, dataLength, messageData);
    if (sendStatus == CAN_OK) {
    Serial.println("Message Sent Successfully!");
    } else {
      Serial.println("Error Sending Message...");
      // Check for error flags
      byte errorFlag = CAN.getError();
      Serial.print("Error Flag: 0x");
      Serial.println(errorFlag, HEX);
    }
    delay(messageInterval);
  }
}

void loop() {
    // Send CAN message
  
  
  // Wait for the specified interval before sending the next message

}