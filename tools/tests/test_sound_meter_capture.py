#!/usr/bin/env python3
"""Source-level microphone ownership and fresh-PCM capture regression checks."""
from pathlib import Path
root = Path(__file__).resolve().parents[2]
def read(name):
    return (root / name).read_text(encoding="utf-8")

board = read("components/board/board.cpp")
hdr = read("components/board/include/board/board.hpp")
meter = read("apps/sound_meter/sound_meter_app.cpp")
processor = read("apps/sound_meter/level_processor.cpp")
for token in (
    "setBufferReleaseCallback(this, microphone_buffer_released)",
    "std::memory_order_release", "std::memory_order_acquire",
    "completed_microphone_buffer_.exchange(", "M5.Mic.record(buffer, count, hz)",
    "M5.Mic.end()", "M5.Speaker.begin()",
    "set_notification_volume_level(notification_volume_level_)",
):
    assert token in board, token
assert "std::atomic<void*> completed_microphone_buffer_" in hdr
assert "microphone_capture_completed(samples_)" in meter
assert "if (!board_.microphone_recording())" not in meter
assert "kCaptureTimeoutMs = 600" in meter
assert "kWarmupBlocks = 2" in meter
assert "kConstantBlockLimit = 8" in meter
assert "low == high && low == static_cast<std::int16_t>(0x5A5A)" in meter
assert "levels_.consume(samples_" in meter
assert meter.index("completed_blocks_ <= kWarmupBlocks") < meter.index("levels_.consume(samples_")
assert "last_rms_ = rms" in processor
assert "kNoiseFloorRms = 180.0F" in processor
assert "kFullScaleRms = 9000.0F" in processor
assert 'M5 RESET MAX | BOCZNY POWROT' in meter
print("Mic capture source-contract regression PASS")
