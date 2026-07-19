#pragma once

#include <cstddef>
#include <cstring>

using byte = unsigned char;

#define PROGMEM
#define memcpy_P(destination, source, length) std::memcpy(destination, source, length)

const int HEX = 16;
const int INPUT = 0;

class SerialStub {
public:
    void begin(unsigned long) {}
    void println() {}

    template <typename T>
    void print(const T&) {}

    template <typename T>
    void print(const T&, int) {}

    template <typename T>
    void println(const T&) {}

    template <typename T>
    void println(const T&, int) {}
};

static SerialStub Serial;

inline void pinMode(int, int) {}
inline int digitalRead(int) { return 1; }
inline unsigned long millis() { return 0; }
inline void delay(unsigned long) {}
