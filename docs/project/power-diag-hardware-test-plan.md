# PowerDiag v0.1 hardware validation

Target: M5Stack M5StickC Plus SE, two-device firmware where communication cases require a peer.

## A — Basic session

- Open NARZEDZIA -> PowerDiag.
- Start a test with M5.
- Wait at least one minute, leave with long BOCZNY, use Launcher, then re-enter.
- Verify TEST continues rather than restarting and the previously accumulated metrics remain.
- Verify M5 during a Running session opens NOWY TEST? and cannot reset without explicit TAK confirmation.

## B — LCD accounting

- Start a fresh test.
- Let Active transition to Dimmed and DisplayOff normally.
- Remain off for several minutes, wake, pass through Clock Glance, and restore PowerDiag.
- Verify LCD OFF increased by approximately the off interval.
- Verify LCD ON (Active + Dimmed) plus LCD OFF approximately matches TEST time.

## C — Communicator OFF baseline

- Start a fresh test with Communicator disabled.
- Spend most of 30–60 minutes in DisplayOff.
- Photograph both pages.
- Verify COMM reports OFF and COMM ON remains near zero.

## D — Communicator ON baseline

- Start a fresh test with Communicator enabled but mostly background/DisplayOff.
- Run for a similar duration to test C.
- Photograph both pages.
- Verify COMM ON tracks the enabled interval and RX shows the background schedule when outside Communicator UI.

## E — Communicator foreground

- Spend several minutes in Communicator UI during a Running test.
- Verify COMM UI increases only for foreground Communicator time while COMM ON continues for the full enabled interval.
- Verify FG/BG RX profile and schedule change observationally with normal Communicator lifecycle; PowerDiag must not cause the change.

## F — RadioLab

- Enter RadioLab during a Running test.
- Use it for several minutes, exit normally, and re-enter PowerDiag.
- Verify RADLAB increased by approximately the foreground interval.
- Verify PowerDiag did not alter RadioLab ownership or messaging pause/resume behavior.

## G — Battery sample reuse

- Compare BAT/START/MIN/DELTA changes over several BatteryGuard sampling intervals.
- Verify no PowerDiag-specific periodic `Board::power_status()` call exists or appears in instrumentation.
- If START occurred before a valid sample, verify first later valid BatteryGuard sample becomes START/current/minimum baseline.
- Verify invalid samples do not replace valid baseline/current/minimum values.

## H — DisplayOff / Clock Glance

- Leave PowerDiag on page 2.
- Enter DisplayOff, wait, wake into Clock Glance, then dismiss glance.
- Verify page 2 is restored and no hidden PowerDiag redraw occurred while DisplayOff.
- Verify TEST/LCD counters continued.

## I — Timer and BatteryGuard overlays

- Run Countdown during a PowerDiag session and allow MINUTNIK KONIEC to cover PowerDiag.
- Dismiss it and verify the same PowerDiag page returns with the session intact.
- Exercise LOW/VERY_LOW advisory similarly and verify dismissal restores PowerDiag without resetting the test.

## J — Communicator incoming

- Leave PowerDiag in the foreground with Communicator enabled.
- Send a preset/response and SYGNAL from the second Stick.
- Verify accepted Communicator presentation has normal priority over PowerDiag.
- Exit Communicator normally; returning to Launcher is acceptable.
- Re-enter PowerDiag and verify the diagnostic session continued throughout.
