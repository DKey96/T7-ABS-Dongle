#pragma once

class EEPROMStub {
public:
    EEPROMStub() { reset(); }

    byte read(int address) { return values[address]; }
    void update(int address, byte value) { values[address] = value; }
    void reset() { std::memset(values, 0, sizeof(values)); }

private:
    byte values[64];
};

static EEPROMStub EEPROM;
