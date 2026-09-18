#include "board/board.hpp"
#include "radiolab/radiolab_app.hpp"
#include "radio/radio.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr std::uint8_t kRadioChannel = 6;

}  // namespace

extern "C" void app_main(void)
{
    nikos::board::Board board;
    board.begin();

    nikos::radio::RadioService radio;
    if (!radio.begin(kRadioChannel, nikos::radio::Mode::Normal)) {
        board.draw_screen(
            "RADIO LAB",
            "RADIO INIT FAILED\n"
            "Check serial log for the failing step.");
        while (true) {
            board.poll_input();
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    nikos::radiolab::RadioLabApp app(board, radio);
    app.begin();

    while (true) {
        app.update();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
