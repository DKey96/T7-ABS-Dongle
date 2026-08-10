# World Raid ABS Restore Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Preserve the Euro 5 TFT rider's saved ABS mode across the ECU's temporary ABS-on status during a kill-switch restart.

**Architecture:** Add a small pure state tracker directly to the TFT sketch so the persisted desired state is distinct from observed ECU status. Exercise that exact tracker through a host C++ replay test, then integrate its decisions with the existing EEPROM and CAN paths without changing message timing.

**Tech Stack:** Arduino C++11, Python `unittest`, host `clang++` syntax/runtime checks.

## Global Constraints

- Change behavior only in `src/abs_dongle_sw_tenere_EURO_5_lcd.ino`.
- Do not add retries or timing changes without a World Raid CAN trace.
- Do not claim issue #12 fixed without on-bike validation.
- Preserve explicit stable ABS-on changes outside restoration.

---

### Task 1: Restart-state tracker and replay

**Files:**
- Create: `tests/abs_restore_state_test.cpp`
- Modify: `tests/test_sketch_safety.py`
- Modify: `src/abs_dongle_sw_tenere_EURO_5_lcd.ino`

**Interfaces:**
- Produces: `AbsRestoreState { byte desiredState; bool restorePending; }`
- Produces: `AbsStatusAction observeAbsStatus(AbsRestoreState&, byte)`
- Produces: `void beginAbsRestore(AbsRestoreState&, byte)`

- [ ] **Step 1: Write the failing host replay**

Create a C++ test that includes the real TFT sketch and asserts literal outcomes for these sequences:

```cpp
AbsRestoreState fullOff = {0x1D, true};
assert(observeAbsStatus(fullOff, 0x01) == ABS_STATUS_IGNORED);
assert(fullOff.desiredState == 0x1D);
assert(observeAbsStatus(fullOff, 0x1D) == ABS_RESTORE_CONFIRMED);

AbsRestoreState stable = {0x1B, false};
assert(observeAbsStatus(stable, 0x01) == ABS_DESIRED_STATE_CHANGED);
assert(stable.desiredState == 0x01);
```

Extend the Python suite to compile and execute `tests/abs_restore_state_test.cpp`.

- [ ] **Step 2: Run the focused test and verify RED**

Run: `python3 -m unittest -v tests.test_sketch_safety.SketchSafetyTests.test_lcd_restart_state_replay`

Expected: FAIL because `AbsRestoreState` and `observeAbsStatus` do not exist.

- [ ] **Step 3: Add the minimal pure state tracker**

Add stable-value validation and the three decisions:

```cpp
enum AbsStatusAction {
    ABS_STATUS_IGNORED,
    ABS_RESTORE_CONFIRMED,
    ABS_DESIRED_STATE_CHANGED
};

struct AbsRestoreState {
    byte desiredState;
    bool restorePending;
};
```

While pending, a differing stable observation is ignored; a matching observation confirms restoration. Outside restoration, a differing stable observation updates the desired state and requests persistence. Initialization and unknown bytes are ignored.

- [ ] **Step 4: Run the focused test and verify GREEN**

Run: `python3 -m unittest -v tests.test_sketch_safety.SketchSafetyTests.test_lcd_restart_state_replay`

Expected: PASS.

- [ ] **Step 5: Commit the tracker test cycle**

```bash
git add tests/abs_restore_state_test.cpp tests/test_sketch_safety.py src/abs_dongle_sw_tenere_EURO_5_lcd.ino
git commit -m "test World Raid ABS restore state"
```

### Task 2: Integrate desired-state restoration

**Files:**
- Modify: `tests/abs_restore_state_test.cpp`
- Modify: `src/abs_dongle_sw_tenere_EURO_5_lcd.ino`

**Interfaces:**
- Consumes: `AbsRestoreState`, `observeAbsStatus`, and `beginAbsRestore` from Task 1.
- Produces: `void saveDesiredAbsState(byte)` for explicit rear/full button selections.

- [ ] **Step 1: Add failing integration-oriented state assertions**

Extend the replay to assert that `beginAbsRestore(state, 0x1B)` replaces the desired value and marks it pending, and that invalid values do not change an existing desired state.

- [ ] **Step 2: Run the focused test and verify RED**

Run: `python3 -m unittest -v tests.test_sketch_safety.SketchSafetyTests.test_lcd_restart_state_replay`

Expected: FAIL on the new begin/invalid-state assertions.

- [ ] **Step 3: Wire tracker decisions into the sketch**

- Initialize the tracker from EEPROM whenever `restoreLastSavedState()` sends a saved mode.
- On `0x11` and `0x1A`, retain the saved preference and begin restoration before sending.
- Ignore divergent status for EEPROM purposes while pending.
- Confirm and clear pending on a matching status.
- Preserve the existing EEPROM update path for stable changes outside restoration.
- Save `0x1B` before sending rear-off and `0x1D` before sending full-off from explicit button sequences.

- [ ] **Step 4: Run the full suite and verify GREEN**

Run: `python3 -m unittest -v tests/test_sketch_safety.py`

Expected: all restart replays, source checks, and all three host sketch syntax checks pass.

- [ ] **Step 5: Commit the integration**

```bash
git add src/abs_dongle_sw_tenere_EURO_5_lcd.ino tests/abs_restore_state_test.cpp tests/test_sketch_safety.py
git commit -m "fix TFT ABS state restoration"
```

### Task 3: Final verification and branch publication

**Files:**
- Verify: all changed files

- [ ] **Step 1: Run full verification**

```bash
python3 -m unittest -v tests/test_sketch_safety.py
git diff --check origin/main...HEAD
git status --short
```

Expected: all tests pass, no whitespace errors, and no uncommitted files.

- [ ] **Step 2: Review scope**

Run: `git diff --stat origin/main...HEAD && git log --oneline origin/main..HEAD`

Expected: design/plan documentation, TFT state tracking, and its regression test only.

- [ ] **Step 3: Push the requested branch**

Run: `git push -u origin codex/fix-world-raid-state-restore`

Expected: the remote branch tracks the verified local commit. Do not open a PR or claim issue #12 fixed without explicit authorization and hardware validation.
