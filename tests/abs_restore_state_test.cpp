#include <cassert>

#include "../src/abs_dongle_sw_tenere_EURO_5_lcd.ino"

int main() {
    AbsRestoreState fullOff = {0x1D, true};
    assert(observeAbsStatus(fullOff, 0x01) == ABS_STATUS_IGNORED);
    assert(fullOff.desiredState == 0x1D);
    assert(fullOff.restorePending);
    assert(observeAbsStatus(fullOff, 0x1D) == ABS_RESTORE_CONFIRMED);
    assert(!fullOff.restorePending);

    AbsRestoreState rearOff = {0x1B, true};
    assert(observeAbsStatus(rearOff, 0x01) == ABS_STATUS_IGNORED);
    assert(rearOff.desiredState == 0x1B);
    assert(observeAbsStatus(rearOff, 0x1B) == ABS_RESTORE_CONFIRMED);

    AbsRestoreState stable = {0x1B, false};
    assert(observeAbsStatus(stable, 0x01) == ABS_DESIRED_STATE_CHANGED);
    assert(stable.desiredState == 0x01);
    assert(!stable.restorePending);

    AbsRestoreState invalid = {0x1D, false};
    assert(observeAbsStatus(invalid, 0x11) == ABS_STATUS_IGNORED);
    assert(invalid.desiredState == 0x1D);

    AbsRestoreState selection = {0x1D, false};
    beginAbsRestore(selection, 0x1B);
    assert(selection.desiredState == 0x1B);
    assert(selection.restorePending);

    beginAbsRestore(selection, 0x18);
    assert(selection.desiredState == 0x1B);
    assert(selection.restorePending);

    EEPROM.reset();
    restoreState = {0x01, true};
    restoreLastSavedState();
    assert(!restoreState.restorePending);

    byte firstStableState[savedDataLength] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x1B
    };
    handleObservedAbsState(firstStableState);
    assert(restoreState.desiredState == 0x1B);
    assert(EEPROM.read(eepromAddress + 5) == 0x1B);

    EEPROM.reset();
    restoreState = {0x1D, true};
    lastState[5] = 0x1D;
    EEPROM.update(eepromAddress + 5, 0x1D);

    byte observed[savedDataLength] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    handleObservedAbsState(observed);
    assert(restoreState.desiredState == 0x1D);
    assert(restoreState.restorePending);
    assert(EEPROM.read(eepromAddress + 5) == 0x1D);

    observed[5] = 0x1D;
    handleObservedAbsState(observed);
    assert(!restoreState.restorePending);
    assert(EEPROM.read(eepromAddress + 5) == 0x1D);

    observed[5] = 0x01;
    handleObservedAbsState(observed);
    assert(restoreState.desiredState == 0x01);
    assert(EEPROM.read(eepromAddress + 5) == 0x01);

    saveDesiredAbsState(0x1B);
    assert(restoreState.desiredState == 0x1B);
    assert(restoreState.restorePending);
    assert(lastState[5] == 0x1B);
    assert(EEPROM.read(eepromAddress + 5) == 0x1B);

    return 0;
}
