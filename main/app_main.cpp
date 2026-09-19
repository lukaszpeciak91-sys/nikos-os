#include "board/board.hpp"
#include "launcher/launcher.hpp"
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
constexpr std::uint32_t kSplashDurationMs = 750;
constexpr std::uint32_t kLoopDelayMs = 20;
constexpr std::uint32_t kRadioErrorDisplayMs = 1200;

enum class RuntimeState : std::uint8_t {
    Launcher,
    RadioLab,
};

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

    nikos::launcher::Launcher launcher(board);
    launcher.show_splash();
    vTaskDelay(pdMS_TO_TICKS(kSplashDurationMs));

    if (!initialize_nvs()) {
        board.draw_screen(
            "Nikoś OS",
            "NVS INIT FAILED\n"
            "Check serial log for the failing step.");
        while (true) {
            board.poll_input();
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    nikos::radio::RadioService radio;
    nikos::radiolab::RadioLabApp radiolab(board, radio);

    RuntimeState state = RuntimeState::Launcher;
    launcher.begin();

    while (true) {
        if (state == RuntimeState::Launcher) {
            if (launcher.update() == nikos::launcher::Action::OpenRadioLab) {
                if (radio.begin(kRadioChannel, nikos::radio::Mode::Normal)) {
                    radiolab.begin();
                    state = RuntimeState::RadioLab;
                } else {
                    board.draw_screen(
                        "RADIO LAB",
                        "RADIO INIT FAILED\n"
                        "Check serial log.");
                    vTaskDelay(pdMS_TO_TICKS(kRadioErrorDisplayMs));
                    launcher.begin();
                }
            }
        } else {
            if (radiolab.update()
                == nikos::radiolab::RadioLabApp::UpdateResult::ExitRequested) {
                radiolab.end();

                if (!radio.stop()) {
                    ESP_LOGW(kTag, "Radio stop completed with cleanup errors");
                }

                launcher.begin();
                state = RuntimeState::Launcher;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
    }
}
