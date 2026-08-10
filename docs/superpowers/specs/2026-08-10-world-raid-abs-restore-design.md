# World Raid ABS Restore State Design

## Goal

Prevent the Euro 5 TFT sketch from replacing the rider's remembered ABS mode with the temporary ABS-on status that the ECU reports during a kill-switch restart.

This is an experimental fix for issue #12. It must not be described as hardware-verified until a 2022 World Raid test confirms the full-ABS-off mode is restored.

## Scope

Only `abs_dongle_sw_tenere_EURO_5_lcd.ino` changes behavior. The monochrome sketches are unchanged because the failing motorcycle report is specific to the Euro 5 TFT/World Raid protocol.

## State Model

The sketch currently uses `lastState` for two different concepts:

- the rider's desired mode persisted in EEPROM;
- the latest mode reported by the ABS ECU.

The new model separates these concepts:

- `desiredState` is the stable protocol byte remembered across restarts (`0x01`, `0x1B`, or `0x1D`);
- `restorePending` indicates that the desired mode has been sent and is waiting for matching ECU confirmation;
- incoming ECU status is observed without changing `desiredState` while restoration is pending.

## Data Flow

1. At setup, read `desiredState` from EEPROM, mark restoration pending, and send its command.
2. On restart markers `0x11` or `0x1A`, keep the same desired state, mark restoration pending, and resend it using the existing timing.
3. While restoration is pending:
   - matching status confirms restoration and clears the pending flag;
   - a different stable status is treated as a transient ECU report and is not persisted.
4. Outside restoration, a stable ECU status change keeps the existing behavior and becomes the new persisted preference.
5. A detected short/long TFT button selection records rear-off/full-off as the desired mode before transmitting its command, then waits for confirmation.

Unknown status and initialization bytes are never persisted as desired modes.

## Failure Handling

The patch does not add arbitrary retries or delays. If the ECU rejects a correctly transmitted full-off command, `restorePending` remains true and the saved preference is retained rather than silently replaced with ABS-on. A future CAN trace can then justify a separate readiness/retry change.

An explicit ABS-on request made during an unresolved restoration cannot yet be distinguished from the ECU's automatic ABS-on transition. That limitation will remain documented until the relevant button/status trace is available.

## Testing

A host-side C++ regression harness will exercise the same state tracker used by the sketch:

- saved full-off `0x1D`, observed temporary on `0x01`, restart, confirmation `0x1D`;
- saved rear-off `0x1B` with the same temporary on transition;
- stable user change outside restoration persists normally;
- explicit rear/full button selection updates the desired mode and waits for confirmation;
- invalid protocol values are ignored.

The existing sketch syntax checks and all source-safety tests must continue to pass.
