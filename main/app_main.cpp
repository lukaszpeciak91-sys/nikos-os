#include "board/board.hpp"
#include "radiolab/radiolab_app.hpp"
#include "radio/radio.hpp"

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

namespace {

constexpr char kTag[] = "main";
constexpr std::uint8_t kRadioChannel = 6;

bool initialize_nvs()
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES
        || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        result = nvs_flash_erase();
        if (result == ESP_OK) {
            result = nvs_flash_init();
        }
    }

    if (result != ESP_OK) {
        ESP_LOGE(kTag, "NVS initialization failed: %s", esp_err_to_name(result));
        return false;
    }

    return true;
}

}  // namespace

extern "C" void app_main(void)
{
    nikos::board::Board board;
    board.begin();

    if (!initialize_nvs()) {
        board.draw_screen(
            "RADIO LAB",
            "NVS INIT FAILED\n"
            "Check serial log for the failing step.");
        while (true) {
            board.poll_input();
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

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
