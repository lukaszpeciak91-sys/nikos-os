#!/usr/bin/env python3
"""Sound Meter USB debug access and microphone failure logging contract."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
main=(root/"main/app_main.cpp").read_text(encoding="utf-8")
board=(root/"components/board/board.cpp").read_text(encoding="utf-8")
meter=(root/"apps/sound_meter/sound_meter_app.cpp").read_text(encoding="utf-8")
start=main.split("action == nikos::launcher::Action::OpenSoundMeter)",1)[1].split(
    "action == nikos::launcher::Action::OpenConverter)",1)[0]
assert "!countdown.expired_pending()" in start
assert "&& (!charging_mode.active" in start
assert "|| charging_mode.owner_override_active)" in start
assert "messaging.pause_transport()" in start
assert "signal_sound.stop();" in start and "board.stop_tone();" in start
assert "resume_messaging_after_meter" in start
assert 'SoundMeter request usb=%u owner_unlocked=%u' in start
assert "charging_cable_event == ChargingCableEvent::Connected" in main
assert "charging_cable_event == ChargingCableEvent::Disconnected" in main
relock=main.split("if (owner_relocked_now && state == RuntimeState::SoundMeter)",1)[1].split(
    "if (charging_mode.active",1)[0]
assert "sound_meter.end();" in relock
assert "messaging.resume_transport()" in relock
assert "countdown.resume()" in relock
assert "state = RuntimeState::Launcher;" in relock
assert "owner_relocked_now = true;" in main
for reason in ("MIC_INIT_FAILED","RECORD_REJECTED","CAPTURE_TIMEOUT",
               "PCM_UNTOUCHED","PCM_CONSTANT"):
    assert "reason="+reason in (board+meter)
assert "pin_data_in = 34" in board
assert "pin_ws = 0" in board
assert "sample_rate = 17000" in board
assert "M5.Mic.begin()" in board
assert "speaker_recovered" in board
assert "Mic teardown complete" in board
assert "completed_blocks_ <= kWarmupBlocks + 3" in meter
assert "accepted_requests_ % 64U" in meter
assert "record_microphone(samples_" in meter
assert "microphone_capture_completed(samples_)" in meter
assert "kCaptureTimeoutMs = 600" in meter
assert "kConstantBlockLimit = 8" in meter
print("Sound Meter USB diagnostic source contracts PASS")
