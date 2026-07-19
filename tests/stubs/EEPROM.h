#pragma once

class EEPROMStub {
public:
    byte read(int) { return 0; }
    void update(int, byte) {}
};

static EEPROMStub EEPROM;
