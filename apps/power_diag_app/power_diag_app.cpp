#include "power_diag/power_diag_app.hpp"

#include <cstddef>
#include <cstdio>

namespace {

void format_hhmmss(
    std::uint64_t elapsed_us,
    char* output,
    std::size_t output_size)
{
    const std::uint64_t total_seconds = elapsed_us / 1000000ULL;
    const std::uint64_t hours = total_seconds / 3600ULL;
    const std::uint64_t minutes = (total_seconds / 60ULL) % 60ULL;
    const std::uint64_t seconds = total_seconds % 60ULL;

    std::snprintf(
        output,
        output_size,
        "%02llu:%02llu:%02llu",
        static_cast<unsigned long long>(hours),
        static_cast<unsigned long long>(minutes),
        static_cast<unsigned long long>(seconds));
}

void format_mmss(
    std::uint64_t elapsed_us,
    char* output,
    std::size_t output_size)
{
    const std::uint64_t total_seconds = elapsed_us / 1000000ULL;
    const std::uint64_t minutes = total_seconds / 60ULL;
    const std::uint64_t seconds = total_seconds % 60ULL;

    std::snprintf(
        output,
        output_size,
        "%llu:%02llu",
        static_cast<unsigned long long>(minutes),
        static_cast<unsigned long long>(seconds));
}

const char* charge_text(nikos::board::ChargeState state)
{
    using nikos::board::ChargeState;
    switch (state) {
        case ChargeState::Charging:
            return "TAK";
        case ChargeState::Discharging:
            return "NIE";
        case ChargeState::Unknown:
        default:
            return "--";
    }
}

void format_battery_current(
    bool supported,
    std::int32_t current_ma,
    char* output,
    std::size_t output_size)
{
    if (!supported) {
        std::snprintf(output, output_size, "--");
        return;
    }

    if (current_ma > 0) {
        std::snprintf(
            output,
            output_size,
            "+%ldmA",
            static_cast<long>(current_ma));
        return;
    }

    std::snprintf(
        output,
        output_size,
        "%ldmA",
        static_cast<long>(current_ma));
}

const char* rx_profile_text(nikos::power_diag::RxProfile profile)
{
    using nikos::power_diag::RxProfile;
    switch (profile) {
        case RxProfile::Foreground:
            return "FG";
        case RxProfile::Background:
            return "BG";
        case RxProfile::Off:
        default:
            return "OFF";
    }
}

const char* radio_mode_text(nikos::power_diag::RadioMode mode)
{
    return mode == nikos::power_diag::RadioMode::Lr
        ? "LR"
        : "NORMAL";
}

void draw_header(
    nikos::board::Board& board,
    const char* page)
{
    board.draw_text_region(
        8,
        4,
        176,
        18,
        "POWER DIAG",
        2,
        nikos::board::DisplayColor::PrimaryText,
        nikos::board::DisplayColor::Background);
    board.draw_text_region(
        204,
        8,
        28,
        12,
        page,
        1,
        nikos::board::DisplayColor::SecondaryText,
        nikos::board::DisplayColor::Background);
}

void draw_footer(nikos::board::Board& board)
{
    board.draw_text_region(
        34,
        122,
        198,
        12,
        "BOCZNY > | DLUGO POWROT",
        1,
        nikos::board::DisplayColor::SecondaryText,
        nikos::board::DisplayColor::Background);
}

}  // namespace

namespace nikos::power_diag {

PowerDiagApp::PowerDiagApp(board::Board& board)
    : board_(board)
{
}

void PowerDiagApp::begin(const Snapshot& snapshot)
{
    view_ = View::Page1;
    confirmation_selection_ = 0;
    rendered_second_valid_ = false;
    render(snapshot);
}

void PowerDiagApp::redraw(const Snapshot& snapshot)
{
    rendered_second_valid_ = false;
    render(snapshot);
}

PowerDiagApp::UpdateResult PowerDiagApp::update(
    const board::InputState& input,
    const Snapshot& snapshot)
{
    if (view_ == View::ConfirmNewTest) {
        if (input.secondary_long) {
            view_ = confirmation_return_view_;
            confirmation_selection_ = 0;
            render(snapshot);
            return UpdateResult::Running;
        }

        if (input.secondary_short) {
            confirmation_selection_ ^= 1U;
            render(snapshot);
            return UpdateResult::Running;
        }

        if (input.primary_short) {
            if (confirmation_selection_ == 1U) {
                view_ = View::Page1;
                confirmation_selection_ = 0;
                return UpdateResult::NewTestRequested;
            }

            view_ = confirmation_return_view_;
            confirmation_selection_ = 0;
            render(snapshot);
        }

        return UpdateResult::Running;
    }

    if (input.secondary_long) {
        return UpdateResult::ExitRequested;
    }

    if (snapshot.state == SessionState::Inactive) {
        if (input.primary_short) {
            view_ = View::Page1;
            return UpdateResult::StartRequested;
        }
        return UpdateResult::Running;
    }

    if (input.secondary_short) {
        view_ = view_ == View::Page1
            ? View::Page2
            : View::Page1;
        render(snapshot);
        return UpdateResult::Running;
    }

    if (input.primary_short) {
        confirmation_return_view_ = view_;
        view_ = View::ConfirmNewTest;
        confirmation_selection_ = 0;
        render(snapshot);
        return UpdateResult::Running;
    }

    if (snapshot.state == SessionState::Running) {
        const std::uint64_t second = snapshot.total_us / 1000000ULL;
        if (!rendered_second_valid_
            || second != rendered_second_
            || snapshot.current_voltage_mv != rendered_voltage_mv_
            || snapshot.current_percent != rendered_percent_) {
            render(snapshot);
        }
    }

    return UpdateResult::Running;
}

void PowerDiagApp::render(const Snapshot& snapshot)
{
    board_.clear_screen();

    if (view_ == View::ConfirmNewTest) {
        render_new_test_confirm();
    } else if (snapshot.state == SessionState::Inactive) {
        render_inactive();
    } else if (view_ == View::Page2) {
        render_usage(snapshot);
    } else {
        render_energy(snapshot);
    }

    rendered_second_valid_ = true;
    rendered_second_ = snapshot.total_us / 1000000ULL;
    rendered_voltage_mv_ = snapshot.current_voltage_mv;
    rendered_percent_ = snapshot.current_percent;
}

void PowerDiagApp::render_inactive()
{
    draw_header(board_, "");

    board_.draw_text_region(
        48,
        42,
        160,
        22,
        "BRAK TESTU",
        2,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        66,
        76,
        130,
        22,
        "M5 START",
        2,
        board::DisplayColor::Accent,
        board::DisplayColor::Background);
    board_.draw_text_region(
        38,
        116,
        190,
        16,
        "BOCZNY DLUGO = POWROT",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void PowerDiagApp::render_energy(const Snapshot& snapshot)
{
    draw_header(board_, "1/2");

    char total_text[16]{};
    format_hhmmss(snapshot.total_us, total_text, sizeof(total_text));

    board_.draw_text_region(
        10, 25, 40, 16, "TEST", 1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        52, 22, 150, 20, total_text, 2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    char battery_text[24] = "--";
    if (snapshot.current_voltage_mv > 0
        && snapshot.current_percent >= 0) {
        std::snprintf(
            battery_text,
            sizeof(battery_text),
            "%dmV %ld%%",
            static_cast<int>(snapshot.current_voltage_mv),
            static_cast<long>(snapshot.current_percent));
    }
    board_.draw_text_region(
        10, 46, 34, 18, "BAT", 1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        48, 43, 180, 20, battery_text, 2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    char start_text[24] = "--";
    if (snapshot.start_voltage_mv > 0
        && snapshot.start_percent >= 0) {
        std::snprintf(
            start_text,
            sizeof(start_text),
            "%dmV %ld%%",
            static_cast<int>(snapshot.start_voltage_mv),
            static_cast<long>(snapshot.start_percent));
    }
    board_.draw_text_region(
        10, 66, 218, 12, "START", 1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        55, 66, 170, 12, start_text, 1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    char min_text[16] = "--";
    if (snapshot.minimum_voltage_mv > 0) {
        std::snprintf(
            min_text,
            sizeof(min_text),
            "%dmV",
            static_cast<int>(snapshot.minimum_voltage_mv));
    }
    board_.draw_text_region(
        10, 79, 218, 12, "MIN", 1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        55, 79, 170, 12, min_text, 1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    char delta_text[18] = "--";
    if (snapshot.start_voltage_mv > 0
        && snapshot.current_voltage_mv > 0) {
        std::snprintf(
            delta_text,
            sizeof(delta_text),
            "%+ldmV",
            static_cast<long>(snapshot.delta_voltage_mv));
    }
    board_.draw_text_region(
        10, 92, 42, 16, "DELTA", 1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        55, 89, 150, 18, delta_text, 2,
        board::DisplayColor::Accent,
        board::DisplayColor::Background);

    char lcd_on[16]{};
    char lcd_off[16]{};
    format_mmss(
        snapshot.lcd_active_us + snapshot.lcd_dimmed_us,
        lcd_on,
        sizeof(lcd_on));
    format_mmss(snapshot.lcd_off_us, lcd_off, sizeof(lcd_off));

    char lcd_text[48]{};
    std::snprintf(
        lcd_text,
        sizeof(lcd_text),
        "LCD ON %s  OFF %s",
        lcd_on,
        lcd_off);
    board_.draw_text_region(
        10, 108, 220, 12, lcd_text, 1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    draw_footer(board_);
}

void PowerDiagApp::render_usage(const Snapshot& snapshot)
{
    draw_header(board_, "2/2");

    char comm_on[16]{};
    char comm_ui[16]{};
    char radiolab[16]{};
    format_mmss(
        snapshot.communicator_on_us,
        comm_on,
        sizeof(comm_on));
    format_mmss(
        snapshot.communicator_ui_us,
        comm_ui,
        sizeof(comm_ui));
    format_mmss(
        snapshot.radiolab_us,
        radiolab,
        sizeof(radiolab));

    char line[64]{};
    std::snprintf(
        line,
        sizeof(line),
        "COMM %s  %s",
        snapshot.observation.communicator_enabled ? "ON" : "OFF",
        comm_on);
    board_.draw_text_region(
        10, 28, 220, 14, line, 1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
    std::snprintf(line, sizeof(line), "COMM UI  %s", comm_ui);
    board_.draw_text_region(
        10, 43, 220, 14, line, 1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
    std::snprintf(line, sizeof(line), "RADLAB   %s", radiolab);
    board_.draw_text_region(
        10, 58, 220, 14, line, 1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    const Observation& observation = snapshot.observation;
    if (observation.rx_profile == RxProfile::Off) {
        std::snprintf(line, sizeof(line), "RX OFF");
    } else {
        std::snprintf(
            line,
            sizeof(line),
            "RX %s  %u/%u",
            rx_profile_text(observation.rx_profile),
            static_cast<unsigned>(observation.rx_interval_ms),
            static_cast<unsigned>(observation.rx_wake_window_ms));
    }
    board_.draw_text_region(
        10, 77, 220, 14, line, 1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    std::snprintf(
        line,
        sizeof(line),
        "MODE %s",
        radio_mode_text(observation.radio_mode));
    board_.draw_text_region(
        10, 93, 95, 14, line, 1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    if (observation.rssi_valid) {
        std::snprintf(
            line,
            sizeof(line),
            "RSSI %d",
            static_cast<int>(observation.rssi));
    } else {
        std::snprintf(line, sizeof(line), "RSSI --");
    }
    board_.draw_text_region(
        112, 93, 116, 14, line, 1,
        observation.peer_reachable
            ? board::DisplayColor::StatusActive
            : board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    const char* peer_text = "--";
    if (observation.peer_reachable) {
        peer_text = "OK";
    } else if (observation.peer_known) {
        peer_text = "ZNANY";
    }

    char current_text[20]{};
    format_battery_current(
        snapshot.battery_current_supported,
        snapshot.battery_current_ma,
        current_text,
        sizeof(current_text));

    std::snprintf(
        line,
        sizeof(line),
        "CHG %s  I %s  PEER %s",
        charge_text(snapshot.charge_state),
        current_text,
        peer_text);
    board_.draw_text_region(
        10, 108, 220, 12, line, 1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    draw_footer(board_);
}

void PowerDiagApp::render_new_test_confirm()
{
    draw_header(board_, "");

    board_.draw_text_region(
        45,
        31,
        160,
        22,
        "NOWY TEST?",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    constexpr const char* kItems[2] = {"NIE", "TAK"};
    for (std::uint8_t index = 0; index < 2; ++index) {
        const bool selected = index == confirmation_selection_;
        const std::int16_t y =
            static_cast<std::int16_t>(65 + index * 24);

        board_.draw_text_region(
            30,
            y,
            180,
            20,
            kItems[index],
            2,
            selected
                ? board::DisplayColor::PrimaryText
                : board::DisplayColor::SecondaryText,
            selected
                ? board::DisplayColor::Surface
                : board::DisplayColor::Background);

        if (selected) {
            board_.draw_text_region(
                14,
                y,
                12,
                20,
                ">",
                2,
                board::DisplayColor::Accent,
                board::DisplayColor::Background);
        }
    }

    board_.draw_text_region(
        24,
        112,
        80,
        11,
        "M5 OK",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        24,
        123,
        210,
        11,
        "BOCZNY ZMIEN | DLUGO ANULUJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

}  // namespace nikos::power_diag
