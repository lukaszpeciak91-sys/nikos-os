# Countdown Timer v0.1 — Hardware Test Plan

Target: two M5Stack M5StickC Plus SE units running the same Timer PR firmware.

Run these checks after a successful ESP-IDF 5.5.5 build and flash.

## A — Duration setup

Open ZEGAR -> MINUTNIK and cycle with BOCZNY short.

Verify the complete sequence:

- 00:30, 01:00, 01:30, ... 04:30, 05:00;
- 06:00, 07:00, ... 15:00;
- 15:00 wraps to 00:30.

Verify there is no 00:00 or arbitrary seconds editor.

## B — Start

Select 00:30 and press M5.

Verify the first active frame shows 00:30 rather than immediately dropping to 00:29.

## C — Pause / Resume

Start a Timer.

1. With PAUZA focused, press M5.
2. Wait several seconds and verify MM:SS does not change.
3. Press M5 on WZNOW.
4. Verify countdown continues from the retained remaining time.

## D — Reset

Start a Timer and choose RESETUJ.

Verify:

- active countdown is cancelled;
- MINUTNIK returns to duration setup;
- the previously configured duration is retained;
- no later KONIEC alert occurs from the reset countdown.

## E — Background

Start a Timer, use POWROT to return to ZEGAR/Main, then enter Communicator.

Verify countdown continues and re-entering MINUTNIK restores the real Running/Paused state rather than a fresh setup.

## F — DisplayOff

Start a Timer and make no further input.

Verify:

- normal Active -> Dimmed -> DisplayOff still occurs;
- countdown continues while LCD is off;
- expiration wakes the LCD to the Timer alert.

## G — Alert

At expiration verify:

- full-screen MINUTNIK / KONIEC is shown;
- the currently selected SYGNAL sound pattern plays once with its existing finite behavior;
- the visual alert remains pending after audio finishes;
- no repeated alarm is triggered by later main-loop iterations.

## H — Dismissal

Dismiss the Timer alert with M5, then repeat using BOCZNY.

Verify:

- Timer audio stops immediately if still playing;
- the pending expiration is acknowledged;
- the previous foreground UI is restored/redrawn;
- the dismissal physical gesture does not execute an underlying SELECT/NEXT/SEND action.

Also allow the pending alert to reach DisplayOff, then press a user button:

- the first DisplayOff wake gesture is consumed;
- Timer alert is shown again instead of Clock Glance;
- a fresh gesture is required to dismiss it.

## I — Clock Glance

With a Timer Running or Paused but not expired:

1. allow DisplayOff;
2. press a user button;
3. verify normal ~4 s Clock Glance still appears.

Then expire a Timer while Clock Glance is visible.

Verify Clock Glance is cancelled immediately and replaced by MINUTNIK / KONIEC.

## J — Communication priority

Arrange Timer expiration close to accepted normal incoming Communicator traffic.

Verify:

- Communicator UI wins;
- Timer alert/audio does not remain on top;
- Timer expiration remains pending;
- after the communication foreground flow returns, MINUTNIK / KONIEC appears.

Repeat with received SYGNAL/RING and verify Communicator sound/UI owns the foreground while Timer remains pending.

## K — RadioLab

Start a Timer and enter RadioLab.

Let Timer expire while RadioLab is active.

Verify:

- MINUTNIK / KONIEC overlays RadioLab;
- RadioLab radio/event/timer processing remains healthy while its rendering is suppressed;
- dismissing Timer restores the current RadioLab UI/state;
- RadioLab session is not restarted;
- existing BOCZNY PING / M5 HELLO footer geometry remains unchanged.
