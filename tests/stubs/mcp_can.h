#pragma once

#define CAN_OK 0
#define MCP_ANY 0
#define CAN_500KBPS 0
#define MCP_8MHZ 0
#define MCP_NORMAL 0

class MCP_CAN {
public:
    explicit MCP_CAN(int) {}
    int begin(int, int, int) { return CAN_OK; }
    void setMode(int) {}
    int sendMsgBuf(unsigned long, byte, byte, const byte*) { return CAN_OK; }
    int readMsgBuf(unsigned long*, byte*, byte*) { return CAN_OK; }
};
