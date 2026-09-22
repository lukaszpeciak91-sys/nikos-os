# Stopwatch v0.1 hardware validation

Target: M5Stack M5StickC Plus SE, 240×135 landscape LCD.

Use the same firmware build on the device used for normal Nikoś OS validation. Confirm normal user-facing terminology remains M5 / BOCZNY.

## A — Start / Stop

1. Open ZEGAR -> STOPER.
2. Verify the initial value is `00:00`.
3. Press M5 and verify START begins counting.
4. Wait several seconds.
5. Press M5 again and verify STOP freezes the displayed elapsed value.

## B — Resume

1. Stop at a known value, for example `00:12`.
2. Wait at least 5 seconds and verify the value remains frozen.
3. With WZNOW focused, press M5.
4. Verify counting resumes from the retained elapsed value rather than zero.

## C — Reset

1. Stop the Stopwatch.
2. Select RESETUJ with BOCZNY and confirm with M5.
3. Verify the screen returns to Idle `00:00` with M5 START.

## D — Running exit

1. Start the Stopwatch.
2. Press BOCZNY short and verify immediate POWROT to ZEGAR.
3. Re-enter STOPER.
4. Verify a fresh session begins at `00:00`.
5. Repeat using BOCZNY long.

## E — Display lifecycle

1. Start the Stopwatch and do not touch controls.
2. Verify the normal display lifecycle still dims at approximately 15 s and reaches DisplayOff at approximately 45 s.
3. Confirm Stopwatch tick updates do not keep the display awake.

## F — Clock Glance

1. Start STOPER and allow the LCD to reach DisplayOff.
2. Press a user button and verify normal Clock Glance appears.
3. Use the second fresh gesture to dismiss Clock Glance.
4. Verify STOPER returns and elapsed time includes the time spent with LCD off / Clock Glance visible.

## G — Countdown Timer alert

1. Start MINUTNIK and then open/start STOPER so the Countdown expires while STOPER is visible.
2. Verify `MINUTNIK / KONIEC` keeps its existing alert priority and selected SYGNAL sound.
3. Dismiss the Timer alert.
4. Verify STOPER is restored.
5. If STOPER was Running, verify elapsed time remained monotonic; if Stopped, verify the stopped value was preserved.

## H — Communicator

1. Start STOPER.
2. Receive accepted Communicator traffic that moves foreground into Communicator.
3. Complete/exit the Communicator foreground flow.
4. Return to ZEGAR -> STOPER.
5. Verify a new Stopwatch session begins at `00:00`.

## I — Orientation

1. Test both PRAWA and LEWA display orientation.
2. Verify ZEGAR's four rows remain readable without footer/time overlap.
3. Verify STOPER controls remain readable and the physical M5 / BOCZNY semantics do not change.
