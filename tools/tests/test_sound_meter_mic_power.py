#!/usr/bin/env python3
"""Regression contract for StickCPlus microphone PMU power callback registration.

M5Unified 0.2.22: M5Unified::_begin_audio only installs the StickCPlus
_mic_enabled callback with config.internal_mic=true. That callback switches
AXP192 LDO0 to 2.8 V on Mic.begin and off on Mic.end.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
board = (root / "components/board/board.cpp").read_text(encoding="utf-8")
meter = (root / "apps/sound_meter/sound_meter_app.cpp").read_text(encoding="utf-8")
main = (root / "main/app_main.cpp").read_text(encoding="utf-8")

init = board.split("bool Board::begin()", 1)[1].split("InputState Board::", 1)[0]
assert "config.internal_mic = true;" in init
assert "config.internal_spk = true;" in init
assert "M5.begin(config);" in init
assert init.index("config.internal_mic = true;") < init.index("M5.begin(config);")
# Enabling the callback does NOT open the recording session at boot.
assert "M5.Mic.begin()" not in init

begin = board.split("bool Board::begin_microphone()", 1)[1].split(
    "bool Board::record_microphone(", 1
)[0]
assert "M5.Speaker.stop();" in begin and "M5.Speaker.end();" in begin
assert "mic_config.pin_data_in = 34;" in begin
assert "mic_config.pin_ws = 0;" in begin
assert "mic_config.sample_rate = 17000;" in begin
assert "M5.Mic.begin()" in begin
assert "M5.Mic.setBufferReleaseCallback(this, microphone_buffer_released);" in begin
assert "M5.Speaker.begin()" in begin  # init failure recovery

record = board.split("bool Board::record_microphone(", 1)[1].split(
    "bool Board::microphone_recording()", 1
)[0]
assert "M5.Mic.record(buffer, count, hz)" in record
assert "completed_microphone_buffer_.store(nullptr" in record
assert "completed_microphone_buffer_.exchange(" in board
assert "std::memory_order_release" in board and "std::memory_order_acquire" in board
assert "setBufferReleaseCallback(nullptr, nullptr)" in board
end = board.split("void Board::end_microphone()", 1)[1].split(
    "void Board::wake_display()", 1
)[0]
assert "M5.Mic.end();" in end and "M5.Speaker.begin()" in end

assert "kConstantBlockLimit = 8" in meter
assert "reason=PCM_CONSTANT" in meter and "reason=PCM_UNTOUCHED" in meter
assert "kCaptureTimeoutMs = 600" in meter
assert "reason=CAPTURE_TIMEOUT" in meter and "reason=RECORD_REJECTED" in meter
assert "microphone_capture_completed(samples_)" in meter
assert "record_microphone(samples_" in meter
assert "sound_meter.end();" in main
assert "messaging.pause_transport()" in main
assert "messaging.resume_transport()" in main
assert "charging_mode.owner_override_active" in main
assert "owner_relocked_now && state == RuntimeState::SoundMeter" in main
print("StickCPlus mic power/capture/lifecycle source contracts PASS")
