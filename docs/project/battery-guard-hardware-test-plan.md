# BatteryGuard v0.1 hardware validation

Target: M5Stack M5StickC Plus SE.

The production constants are initial hardware-validation values. For practical bench testing, temporarily raising percentage/voltage thresholds is acceptable on a disposable test branch/commit; restore production constants before merge.

## A — LOW advisory

- Reach or temporarily emulate <=20% while not charging.
- Verify `NISKA BATERIA / PODLACZ LADOWARKE` appears once.
- Dismiss with M5 or BOCZNY.
- Keep battery below 20% through multiple 10 s samples and verify it does not repeat.
- Recover above 25%, descend again, and verify LOW re-arms.

## B — DisplayOff LOW

- Enter DisplayOff.
- Cross/test LOW.
- Verify LCD remains asleep.
- Wake normally and verify the pending advisory is presented without leaking the wake/dismiss gesture into the underlying UI.

## C — VERY_LOW

- Cross/test <=10% while not charging.
- Verify `BARDZO NISKA BATERIA` replaces/supersedes a pending LOW warning.
- Verify it does not repeat continuously.
- Recover above 15% before testing a second descent.

## D — Charging

- Connect USB and confirm M5Unified reports Charging.
- Verify LOW/VERY_LOW are not newly queued.
- Verify any one-sample critical confirmation is cancelled.
- Verify no automatic shutdown occurs while Charging.

## E — Critical transient

- Temporarily raise the critical voltage threshold if needed.
- Produce exactly one critical sample, then recover above the threshold before the next sample.
- Verify no shutdown and that confirmation resets.

## F — Confirmed critical

- Keep voltage at/below the test threshold across two consecutive safety samples.
- Verify LCD wakes if necessary.
- Verify `NISKA BATERIA / WYLACZAM...` appears for about 1.75 s.
- Verify sound stops, Communicator/messaging and radio are cleaned up, and controlled power-off follows.

## G — Countdown

- Run MINUTNIK while LOW/VERY_LOW becomes pending.
- Verify Countdown continues unchanged.
- If KONIEC occurs, verify Timer alert appears before the battery advisory.
- Dismiss Timer and verify pending battery advisory can appear afterward.

## H — Communicator

- Keep Communicator enabled while an advisory is pending/visible.
- Send a normal message and SYGNAL from the second Stick.
- Verify communication presentation takes priority and the battery advisory remains pending for later.

## I — Stopwatch and RadioLab

- Run STOPER through a battery advisory, dismiss it, and verify elapsed time/session are preserved.
- Enter RadioLab, show advisory, and verify RadioLab RX/events/timers continue underneath without visible rendering until restore.

## J — DisplayOff / POWER regression

- Verify PR #37 hidden-work suppression remains unchanged apart from the intentional ~10 s BatteryGuard sample.
- Verify short POWER still performs display off/wake only.
- Verify long POWER remains board/PMIC behavior.
- Verify Launcher WYLACZ still uses controlled whole-device shutdown.
