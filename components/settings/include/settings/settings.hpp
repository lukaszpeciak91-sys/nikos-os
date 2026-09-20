#pragma once

#include <cstdint>

namespace nikos::settings {

enum class SignalSound : std::uint8_t {
    Gentle,
    Classic,
    Pager,
};

struct State {
    SignalSound signal_sound = SignalSound::Gentle;
};

}  // namespace nikos::settings
