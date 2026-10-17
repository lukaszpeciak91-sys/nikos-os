#include "sound_meter/level_processor.hpp"
#include <cassert>
#include <cstdint>
using nikos::sound_meter::LevelProcessor;
int main() {
    LevelProcessor processor;
    std::int16_t quiet[256]{};
    processor.consume(quiet, 256, 100);
    assert(processor.level() == 0);
    assert(processor.peak() == 0);
    assert(processor.last_rms() == 0.0F);
    std::int16_t loud[256]{};
    for (int i = 0; i < 256; ++i) loud[i] = i % 2 ? 14000 : -14000;
    processor.consume(loud, 256, 200);
    assert(processor.level() > 50 && processor.level() <= 100);
    assert(processor.last_rms() > 13999.0F && processor.last_rms() < 14001.0F);
    std::int16_t biased[256]{};
    for (int i = 0; i < 256; ++i)
        biased[i] = i % 2 ? 2000 : 0;
    processor.consume(biased, 256, 250);
    assert(processor.last_rms() > 999.0F && processor.last_rms() < 1001.0F);
    auto before = processor.level();
    processor.consume(quiet, 256, 300);
    assert(processor.level() < before);
    assert(processor.peak() >= processor.level());
    for (std::uint32_t t = 400; t < 3000; t += 80) processor.consume(quiet, 256, t);
    assert(processor.peak() < before);
    processor.reset_peak();
    assert(processor.peak() == processor.level());
    processor.reset();
    assert(processor.level() == 0);
}
