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

nikos::messaging::Config make_messaging_config(
    nikos::radio::Mode mode)
{
    nikos::messaging::Config config;
    config.channel = kRadioChannel;
    config.mode = mode;
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

    bool communicator_enabled = false;
    bool resume_messaging_after_radiolab = false;

    RuntimeState state = RuntimeState::Launcher;
    launcher.begin(communicator_enabled);

    while (true) {
        // update() is a no-op while messaging is disabled or while RadioLab
        // temporarily owns the radio transport.
        messaging.update();

        if (state == RuntimeState::Launcher) {
            if (communicator_enabled
                && communicator.process_incoming()) {
                if (!communicator.begin()) {
                    ESP_LOGW(
                        kTag,
                        "Communicator foreground RX profile could not be applied");
                }
                state = RuntimeState::Communicator;
            } else {
                const nikos::launcher::Action action = launcher.update();

                if (action == nikos::launcher::Action::StartCommunicator) {
                    communicator.reset_session();

                    if (messaging.begin(
                            make_messaging_config(
                                messaging.radio_mode()))) {
                        communicator_enabled = true;

                        if (!communicator.begin()) {
                            ESP_LOGW(
                                kTag,
                                "Communicator foreground RX profile could not be applied");
                        }
                        state = RuntimeState::Communicator;
                    } else {
                        ESP_LOGW(
                            kTag,
                            "Communicator messaging failed to start");
                        launcher.begin(false);
                    }
                } else if (
                    action == nikos::launcher::Action::OpenCommunicator) {
                    if (!communicator.begin()) {
                        ESP_LOGW(
                            kTag,
                            "Communicator foreground RX profile could not be applied");
                    }
                    state = RuntimeState::Communicator;
                } else if (
                    action == nikos::launcher::Action::StopCommunicator) {
                    communicator.reset_session();

                    if (!messaging.stop()) {
                        ESP_LOGW(
                            kTag,
                            "Communicator messaging stop completed with cleanup errors");
                    }

                    communicator_enabled = false;
                    launcher.begin(false);
                } else if (
                    action == nikos::launcher::Action::ShutdownRequested) {
                    board.stop_tone();
                    communicator.reset_session();

                    if (!messaging.stop()) {
                        ESP_LOGW(
                            kTag,
                            "Messaging stop completed with cleanup errors during shutdown");
                    }

                    communicator_enabled = false;
                    resume_messaging_after_radiolab = false;

                    if (!radio.stop()) {
                        ESP_LOGW(
                            kTag,
                            "Radio stop completed with cleanup errors during shutdown");
                    }

                    board.power_off();

                    // M5Unified powerOff() should not return on target hardware.
                    while (true) {
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                } else if (
                    action == nikos::launcher::Action::OpenRadioLab) {
                    resume_messaging_after_radiolab =
                        communicator_enabled;

                    if (resume_messaging_after_radiolab
                        && !messaging.pause_transport()) {
                        ESP_LOGW(
                            kTag,
                            "Messaging transport pause completed with cleanup errors");
                    }

                    if (radio.begin(
                            kRadioChannel,
                            nikos::radio::Mode::Normal)) {
                        radiolab.begin();
                        state = RuntimeState::RadioLab;
                    } else {
                        board.draw_screen(
                            "RADIO LAB",
                            "RADIO INIT FAILED\n"
                            "Check serial log.");
                        vTaskDelay(pdMS_TO_TICKS(kRadioErrorDisplayMs));

                        if (resume_messaging_after_radiolab
                            && !messaging.resume_transport()) {
                            ESP_LOGW(
                                kTag,
                                "Messaging transport failed to resume after RadioLab error; disabling Communicator session");
                            communicator.reset_session();
                            (void)messaging.stop();
                            communicator_enabled = false;
                        }

                        resume_messaging_after_radiolab = false;
                        launcher.begin_tools(communicator_enabled);
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

                launcher.begin(communicator_enabled);
                state = RuntimeState::Launcher;
            }
        } else {
            if (radiolab.update()
                == nikos::radiolab::RadioLabApp::UpdateResult::ExitRequested) {
                radiolab.end();

                if (!radio.stop()) {
                    ESP_LOGW(
                        kTag,
                        "Radio stop completed with cleanup errors");
                }

                if (resume_messaging_after_radiolab
                    && !messaging.resume_transport()) {
                    ESP_LOGW(
                        kTag,
                        "Messaging transport failed to resume after RadioLab; disabling Communicator session");
                    communicator.reset_session();
                    (void)messaging.stop();
                    communicator_enabled = false;
                }

                resume_messaging_after_radiolab = false;
                launcher.begin_tools(communicator_enabled);
                state = RuntimeState::Launcher;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
    }

}