#include "board/board.hpp"
#include "communicator/communicator_app.hpp"
#include "launcher/launcher.hpp"
#include "messaging/messaging_service.hpp"
#include "power/display_lifecycle.hpp"
#include "radiolab/radiolab_app.hpp"
#include "radio/radio.hpp"
#include "settings/settings.hpp"
#include "signal_sound/signal_sound_player.hpp"

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

    // Experimental logical-delivery policy. Keep these together so hardware
    // tests can tune sender behavior without changing delivery architecture.
    config.retry_interval_ms = 1000;
    config.retry_jitter_ms = 250;
    config.max_send_attempts = 8;
    config.delivery_timeout_ms = 12000;

    // Experimental TxResult pacing. MAC success only lengthens the wait for
    // the application ACK; it never completes logical delivery.
    config.tx_result_timeout_ms = 500;
    config.mac_success_ack_grace_margin_ms = 250;

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

    nikos::power::DisplayLifecycle display_lifecycle(board);
    display_lifecycle.begin();

    nikos::settings::State settings;
    board.set_theme(settings.theme);

    nikos::signal_sound::Player signal_sound(board, settings);
    nikos::launcher::Launcher launcher(
        board,
        settings,
        signal_sound);
    launcher.show_splash();
    vTaskDelay(pdMS_TO_TICKS(kSplashDurationMs));

    if (!initialize_nvs()) {
        board.draw_screen(
            "NIKOS OS",
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
        display_lifecycle,
        signal_sound,
        "DRUGI M5");
    nikos::radiolab::RadioLabApp radiolab(board, radio);

    bool communicator_enabled = false;
    bool resume_messaging_after_radiolab = false;

    RuntimeState state = RuntimeState::Launcher;
    display_lifecycle.note_visible_activity();
    launcher.begin(communicator_enabled);

    while (true) {
        // update() advances delivery only while messaging owns active
        // transport. A RadioLab handoff intentionally freezes retry/deadline
        // timing until messaging transport resumes.
        messaging.update();
        signal_sound.update();

        const nikos::board::InputState input =
            display_lifecycle.filter_input(board.poll_input());
        display_lifecycle.update();

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
                const nikos::launcher::Action action = launcher.update(input);

                if (action == nikos::launcher::Action::StartCommunicator) {
                    communicator.reset_session();

                    if (messaging.begin(
                            make_messaging_config(
                                messaging.radio_mode()))) {
                        communicator_enabled = true;
                        display_lifecycle.note_visible_activity();

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
                    display_lifecycle.note_visible_activity();
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
                    display_lifecycle.note_visible_activity();
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
                        display_lifecycle.note_visible_activity();
                        radiolab.begin();
                        state = RuntimeState::RadioLab;
                    } else {
                        display_lifecycle.note_visible_activity();
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
            if (communicator.update(input)
                == nikos::communicator::CommunicatorApp::UpdateResult::ExitRequested) {
                if (!communicator.end()) {
                    ESP_LOGW(
                        kTag,
                        "Communicator background RX profile could not be restored");
                }

                display_lifecycle.note_visible_activity();
                launcher.begin(communicator_enabled);
                state = RuntimeState::Launcher;
            }
        } else {
            if (radiolab.update(input)
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
                display_lifecycle.note_visible_activity();
                launcher.begin_tools(communicator_enabled);
                state = RuntimeState::Launcher;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
    }

}