#include "board/board.hpp"
#include "communicator/communicator_app.hpp"
#include "launcher/launcher.hpp"
#include "messaging/messaging_service.hpp"
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
    Communicator,
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

nikos::messaging::Config make_messaging_config()
{
    nikos::messaging::Config config;
    config.channel = kRadioChannel;
    config.mode = nikos::radio::Mode::Normal;
    config.presence_interval_ms = 2000;
    config.presence_jitter_ms = 250;
    config.retry_interval_ms = 500;

    // Experimental receive/reachability profiles. These are configuration
    // values for validation, not permanent platform timing policy.
    config.foreground_rx = {1000, 500, 7000};
    config.background_rx = {3000, 500, 20000};
    return config;
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
    nikos::messaging::Service messaging(radio);
    nikos::communicator::CommunicatorApp communicator(
        board,
        messaging,
        "DRUGI M5");
    nikos::radiolab::RadioLabApp radiolab(board, radio);

    if (!messaging.begin(make_messaging_config())) {
        ESP_LOGW(
            kTag,
            "Messaging transport failed to start; shell remains available");
    }

    RuntimeState state = RuntimeState::Launcher;
    launcher.begin();

    while (true) {
        // Long-lived messaging state is independent of foreground UI.
        // update() becomes a no-op while RadioLab has transport ownership.
        messaging.update();

        if (state == RuntimeState::Launcher) {
            nikos::messaging::IncomingMessage incoming;
            if (messaging.poll_incoming(incoming)
                && communicator.accept_incoming(incoming)) {
                if (!communicator.begin()) {
                    ESP_LOGW(
                        kTag,
                        "Communicator foreground RX profile could not be applied");
                }
                state = RuntimeState::Communicator;
            } else {
                const nikos::launcher::Action action = launcher.update();

                if (action == nikos::launcher::Action::OpenCommunicator) {
                    if (!communicator.begin()) {
                        ESP_LOGW(
                            kTag,
                            "Communicator foreground RX profile could not be applied");
                    }
                    state = RuntimeState::Communicator;
                } else if (action == nikos::launcher::Action::OpenRadioLab) {
                    if (!messaging.pause_transport()) {
                        ESP_LOGW(
                            kTag,
                            "Messaging transport pause completed with cleanup errors");
                    }

                    if (radio.begin(kRadioChannel, nikos::radio::Mode::Normal)) {
                        radiolab.begin();
                        state = RuntimeState::RadioLab;
                    } else {
                        board.draw_screen(
                            "RADIO LAB",
                            "RADIO INIT FAILED\n"
                            "Check serial log.");
                        vTaskDelay(pdMS_TO_TICKS(kRadioErrorDisplayMs));

                        if (!messaging.resume_transport()) {
                            ESP_LOGW(
                                kTag,
                                "Messaging transport failed to resume after RadioLab error");
                        }

                        launcher.begin();
                    }
                }
            }
        } else if (state == RuntimeState::Communicator) {
            if (communicator.update()
                == nikos::communicator::CommunicatorApp::UpdateResult::ExitRequested) {
                if (!communicator.end()) {
                    ESP_LOGW(
                        kTag,
                        "Communicator background RX profile could not be restored");
                }
                launcher.begin();
                state = RuntimeState::Launcher;
            }
        } else {
            if (radiolab.update()
                == nikos::radiolab::RadioLabApp::UpdateResult::ExitRequested) {
                radiolab.end();

                if (!radio.stop()) {
                    ESP_LOGW(kTag, "Radio stop completed with cleanup errors");
                }

                if (!messaging.resume_transport()) {
                    ESP_LOGW(
                        kTag,
                        "Messaging transport failed to resume after RadioLab");
                }

                launcher.begin();
                state = RuntimeState::Launcher;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
    }
}
