# Short POWER display toggle v0.1 hardware validation

Target: M5Stack M5StickC Plus SE.

## A — Basic toggle
- From Launcher, short POWER and verify immediate DisplayOff.
- Short POWER again and verify Clock Glance appears.

## B — Dimmed
- Wait for Dimmed, short POWER, and verify direct DisplayOff.

## C — No action leakage
- Highlight an actionable Launcher row and short POWER; verify it is not opened/executed.
- Repeat inside Communicator and RadioLab.

## D — Stopwatch
- Start STOPER, short POWER, wait about 10 s, wake with POWER, dismiss Clock Glance with a normal M5/BOCZNY gesture, and verify elapsed time includes the hidden period.

## E — Countdown
- Start a short MINUTNIK, POWER off, and verify expiration still wakes and presents KONIEC.

## F — Pending Timer alert
- Let MINUTNIK KONIEC appear, short POWER, verify LCD goes off without acknowledging expiration, wake again, and verify KONIEC returns.

## G — Communicator
- Enable Communicator and POWER off.
- From the second Stick send a normal message and SYGNAL; verify existing wake/UI/audio behavior.

## H — RadioLab
- Enter RadioLab, POWER off, keep peer traffic active, wake and restore, and verify retained processing/session state.

## I — Long POWER
- Perform one controlled verification that hardware long-power behavior was not replaced by Nikoś OS navigation or actions.
