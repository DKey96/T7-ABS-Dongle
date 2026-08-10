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

    return 0;
}
