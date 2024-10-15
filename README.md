# Tenere 700 ABS Dongle

- This project is all about development of ABS Dongle (memory) for Yamaha Tenere 700 2023 onwards.

## Structure of the project

- src &#8592; Module contains multiple versions of ABS Dongle SW, which handles whole CAN-bus manipulation and control.
- hw &#8592; Module contains information about used hardware, electronic components and schematics for a custom board

### src module

- This module contains files versioned by the name `nano_vX.ino` where the greates number means the newest version (yes I know its not the best versioning, can be replaced by tags in the future if it will make sense). The code was written in C++ and was aimed to work on Atmega328PU used on [Arduino Nano](https://docs.arduino.cc/hardware/nano/), on which it was developed.

### hw module

- This module contains list of Arduino modules used in combination with Arduino Nano, and electronic components + schematic used for development of custom board which can be direct replacement for other ABS dongles.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
