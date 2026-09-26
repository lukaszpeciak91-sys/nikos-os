#pragma once

#include <cstdint>

#include "board/board.hpp"
#include "power_diag/power_diag_session.hpp"

namespace nikos::power_diag {

class PowerDiagApp final {
public:
    enum class UpdateResult : std::uint8_t {
        Running,
        StartRequested,
        NewTestRequested,
        ExitRequested,
    };

    explicit PowerDiagApp(board::Board& board);

    void begin(const Snapshot& snapshot);
    void redraw(const Snapshot& snapshot);
    UpdateResult update(
        const board::InputState& input,
        const Snapshot& snapshot);

private:
    enum class View : std::uint8_t {
        Page1,
        Page2,
        ConfirmNewTest,
    };

    void render(const Snapshot& snapshot);
    void render_inactive();
    void render_energy(const Snapshot& snapshot);
    void render_usage(const Snapshot& snapshot);
    void render_new_test_confirm();

    board::Board& board_;
    View view_ = View::Page1;
    View confirmation_return_view_ = View::Page1;
    std::uint8_t confirmation_selection_ = 0;

    bool rendered_second_valid_ = false;
    std::uint64_t rendered_second_ = 0;
    std::int16_t rendered_voltage_mv_ = -2;
    std::int32_t rendered_percent_ = -2;
    std::uint32_t rendered_current_sample_count_ = 0;
};

}  // namespace nikos::power_diag
