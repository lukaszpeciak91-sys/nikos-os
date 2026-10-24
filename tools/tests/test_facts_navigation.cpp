#include "facts/facts_app.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>

using nikos::board::InputState;
using nikos::facts::FactsApp;

extern "C" std::uint32_t esp_random() {
    static std::uint32_t value = 100;
    return value += 17U;
}

InputState m5_short() {
    InputState input{};
    input.primary_short = true;
    return input;
}
InputState side_short() {
    InputState input{};
    input.secondary_short = true;
    return input;
}
InputState side_long() {
    InputState input{};
    input.secondary_long = true;
    return input;
}

int page_count(const std::string& progress) {
    const auto slash = progress.find('/');
    assert(slash != std::string::npos);
    return std::stoi(progress.substr(slash + 1));
}

int main() {
    nikos::board::Board board;
    FactsApp app(board);
    app.begin();
    assert(board.bounds_ok);
    assert(board.footer_text == "M5 WYBIERZ | SIDE DALEJ");
    assert(app.update(side_long()) == FactsApp::UpdateResult::ExitRequested);

    // Category lists: SIDE moves focus, M5 opens, long SIDE returns to Facts.
    app.begin();
    app.update(side_short()); // KOSMOS.
    app.update(m5_short());  // Category title list.
    assert(board.list_position == "001/101");
    assert(board.footer_text == "M5 OTWORZ | SIDE DALEJ");

    for (int i = 0; i < 8; ++i) app.update(side_short());
    assert(board.list_position == "009/101");
    const int selected_marker = board.marker_y;
    assert(selected_marker >= 28);
    app.update(m5_short());
    assert(board.reader_counter == "009/100");
    assert(board.reader_side == "SIDE LISTA");
    app.redraw(); // Interruption / overlay redraw keeps the card state.
    assert(board.reader_counter == "009/100");
    assert(board.reader_side == "SIDE LISTA");
    app.update(side_short());
    assert(board.list_position == "009/101");
    assert(board.marker_y == selected_marker);
    app.update(m5_short());
    assert(board.reader_counter == "009/100");
    app.update(side_long());
    assert(board.list_position == "009/101");
    assert(board.marker_y == selected_marker);
    app.update(side_long());
    assert(board.footer_text == "M5 WYBIERZ | SIDE DALEJ");

    // Reader: keep existing M5 pagination and advance to the next fact.
    app.begin();
    app.update(side_short()); // KOSMOS.
    app.update(m5_short());  // Title list.
    app.update(m5_short());  // Fact 1.
    assert(board.reader_counter == "001/100");
    assert(board.reader_side == "SIDE LISTA");
    const int pages = page_count(board.reader_page);
    assert(pages > 1); // First approved Space fact spans multiple pages.
    for (int n = 2; n <= pages; ++n) {
        app.update(m5_short());
        assert(board.reader_page == std::to_string(n) + "/" + std::to_string(pages));
        assert(board.reader_counter == "001/100");
    }
    const std::string last_page = board.reader_page;
    const std::string last_action = board.reader_m5;
    app.redraw();
    assert(board.reader_page == last_page && board.reader_m5 == last_action);
    assert(board.reader_side == "SIDE LISTA");
    app.update(m5_short());  // Next fact after final page.
    assert(board.reader_counter == "002/100");
    assert(board.reader_page.rfind("1/", 0) == 0);
    app.update(side_long());
    assert(board.list_position == "002/101");
    assert(board.footer_text == "M5 OTWORZ | SIDE DALEJ");

    // Random: both SIDE presses go to main, M5 preserves page/random flow.
    app.begin();
    app.update(m5_short()); // LOSUJ.
    assert(board.reader_header == "LOSUJ");
    assert(board.reader_side == "SIDE MENU");
    assert(board.reader_counter.find("/600") != std::string::npos);
    const std::string random_counter = board.reader_counter;
    const int random_pages = page_count(board.reader_page);
    for (int n = 2; n <= random_pages; ++n) {
        app.update(m5_short());
        assert(board.reader_counter == random_counter);
        assert(board.reader_side == "SIDE MENU");
    }
    assert(board.reader_m5 == "M5 LOSUJ");
    app.update(m5_short()); // New random fact.
    assert(board.reader_header == "LOSUJ");
    assert(board.reader_side == "SIDE MENU");
    assert(board.reader_counter != random_counter);
    const std::string redraw_page = board.reader_page;
    app.redraw();
    assert(board.reader_page == redraw_page);
    assert(board.reader_side == "SIDE MENU");
    InputState power{};
    power.power_short = true;
    assert(app.update(power) == FactsApp::UpdateResult::Running);
    assert(board.reader_header == "LOSUJ");
    app.update(side_short());
    assert(board.footer_text == "M5 WYBIERZ | SIDE DALEJ");
    app.update(m5_short());  // Enter random again.
    app.update(side_long());
    assert(board.footer_text == "M5 WYBIERZ | SIDE DALEJ");

    // Quiz input path is unchanged: SIDE short stays inside Quiz, long exits.
    app.begin();
    for (int i = 0; i < 7; ++i) app.update(side_short());
    app.update(m5_short());
    assert(board.quiz_header == "QUIZ 01/10");
    app.update(side_short()); // Toggle answer, not navigate out of Quiz.
    assert(board.quiz_header == "QUIZ 01/10");
    app.update(side_long());
    assert(board.footer_text == "M5 WYBIERZ | SIDE DALEJ");

    app.end();
    assert(app.update(m5_short()) == FactsApp::UpdateResult::ExitRequested);
    app.begin();
    assert(board.footer_text == "M5 WYBIERZ | SIDE DALEJ");
    assert(board.bounds_ok);
    std::puts("Facts navigation host regression PASS");
}
