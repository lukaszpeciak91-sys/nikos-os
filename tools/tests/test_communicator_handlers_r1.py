#!/usr/bin/env python3
"""R1: compile and execute production Communicator event handlers with minimal fakes."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
src = (root / "apps/communicator/communicator_app.cpp").read_text(encoding="utf-8")
main = (root / "main/app_main.cpp").read_text(encoding="utf-8")
compiler = shutil.which("g++") or shutil.which("clang++")
assert compiler, "Host C++17 compiler required"

def method(name: str, result: str = "void") -> str:
    marker = f"{result} CommunicatorApp::{name}("
    assert src.count(marker) == 1, (name, src.count(marker))
    start = src.index(marker)
    open_brace = src.index("{", start)
    depth = 0
    end = None
    for idx in range(open_brace, len(src)):
        if src[idx] == "{":
            depth += 1
        elif src[idx] == "}":
            depth -= 1
            if depth == 0:
                end = idx + 1
                break
    assert end is not None, name
    return src[start:end].replace("CommunicatorApp::", "Harness::", 1)

# These are the unmodified production handler bodies, compiled against fakes.
methods = "\n\n".join(method(name) for name in (
    "begin",
    "reset_session",
    "handle_main_input",
    "handle_options_input",
    "handle_incoming_preset_input",
    "handle_response_choice_input",
    "handle_incoming_response_input",
    "handle_wait_decision_input",
)) + "\n\n" + method("send_signal", "bool")

cpp_head = r'''
#include <cassert>
#include <cstdint>
#include <vector>
#include <string>
#define ESP_LOGE(...) ((void)0)
namespace board {
struct InputState { bool primary_short=false, secondary_short=false; };
}
namespace radio {
enum class Mode : std::uint8_t { Normal, Lr };
}
namespace settings {
enum class CommunicatorView : std::uint8_t { List, Single, SignalOnly };
struct State { CommunicatorView communicator_view=CommunicatorView::List; };
}
namespace catalogue {
enum class PresetId { Greeting };
enum class ResponseId { GreetingHello };
struct ResponseSet { std::uint8_t count=0; };
}
constexpr std::uint8_t kMainSignalIndex=5U;
constexpr std::uint8_t kMainOptionsIndex=6U;
constexpr std::uint8_t kMainReturnIndex=7U;
constexpr std::uint8_t kMainChoiceCount=8U;
constexpr std::uint8_t kNoMainActionIndex=0xffU;
struct FakeStore {
    std::vector<std::string>& events;
    std::uint8_t view=0, mode=0;
    int view_saves=0, radio_saves=0, fail_radio_at=0;
    bool fail_view=false;
    bool save_view(std::uint8_t next) {
        ++view_saves; events.push_back("nvs_view");
        if(fail_view) return false;
        view=next; return true;
    }
    bool save_radio_mode(std::uint8_t next) {
        ++radio_saves; events.push_back("nvs_radio");
        if(radio_saves==fail_radio_at) return false;
        mode=next; return true;
    }
};
struct FakeMessaging {
    std::vector<std::string>& events;
    radio::Mode mode=radio::Mode::Normal;
    int mode_calls=0, ring_calls=0, preset_calls=0;
    int fail_mode_at=0;
    bool peer=true, discovery=true;
    bool peer_known() const { return peer; }
    radio::Mode radio_mode() const { return mode; }
    bool set_radio_mode(radio::Mode next) {
        ++mode_calls; events.push_back("radio");
        if(mode_calls==fail_mode_at) return false;
        mode=next; discovery=false; return true;
    }
    bool send_ring() { ++ring_calls; return true; }
};
struct FakeTone { int stops=0; void stop(){ ++stops; } };
enum class DeliveryFeedback { None };
struct Harness {
    enum class State { Main, IncomingPreset, ChoosingResponse,
        IncomingResponse, WaitDecision };
    settings::State& settings_;
    FakeMessaging& messaging_;
    FakeStore& preferences_store_;
    FakeTone signal_sound_{};
    bool active_=false, foreground_exit_requested_=false, options_active_=false;
    bool radio_mode_change_failed_=false, preference_save_failed_=false;
    bool radio_preference_inconsistent_=false;
    State state_=State::Main;
    std::uint8_t selected_main_index_=0, selected_options_index_=0,
        selected_response_index_=0;
    catalogue::PresetId incoming_preset_=catalogue::PresetId::Greeting;
    catalogue::ResponseId incoming_response_=catalogue::ResponseId::GreetingHello;
    catalogue::ResponseSet response_set_{};
    std::uint32_t latest_outgoing_message_id_=0, delivery_result_started_ms_=0;
    DeliveryFeedback latest_delivery_feedback_=DeliveryFeedback::None;
    std::uint8_t latest_delivery_main_action_index_=kNoMainActionIndex;
    bool signal_unavailable_feedback_=false, signal_alert_active_=false;
    bool signal_return_to_launcher_=false, charging_incoming_mode_=false;
    bool signal_audio_complete_rendered_=false, signal_animation_wide_=false;
    std::uint8_t signal_playback_cycles_started_=0;
    std::uint32_t signal_last_animation_ms_=0;
    bool rendered_peer_state_valid_=false, rendered_peer_known_=false,
        rendered_peer_reachable_=false;
    std::uint8_t rendered_signal_bars_=0;
    int main_renders=0, option_renders=0, ring_feedback=0, fake_responses=0;
    std::string footer{};
    void render_current(){ render_main(); }
    void render_main(){ ++main_renders; }
    void render_options(){
        ++option_renders;
        footer=radio_preference_inconsistent_ ? "RADIO/NVS NIESPOJNE"
            : preference_save_failed_ ? "NIE ZAPISANO"
            : radio_mode_change_failed_ ? "NIE UDALO SIE" : "OK";
    }
    void render_signal_unavailable(){}
    void render_response_choices(){}
    void render_wait_decision(){}
    void render_incoming_response(){}
    bool send_selected_preset(){ ++messaging_.preset_calls; return true; }
    bool send_selected_response(){ ++fake_responses; return true; }
    bool send_wait_followup(){ return true; }
    void track_latest_send(std::uint8_t=kNoMainActionIndex){ ++ring_feedback; }
    void begin();
    void reset_session();
    void handle_main_input(const board::InputState&);
    void handle_options_input(const board::InputState&);
    void handle_incoming_preset_input(const board::InputState&);
    void handle_response_choice_input(const board::InputState&);
    void handle_incoming_response_input(const board::InputState&);
    void handle_wait_decision_input(const board::InputState&);
    bool send_signal();
};
'''
cpp_tail = r'''
board::InputState m5(){ board::InputState a{};a.primary_short=true;return a;}
board::InputState side(){board::InputState a{};a.secondary_short=true;return a;}
int main(){
    using View=settings::CommunicatorView;
    using Mode=radio::Mode;
    std::vector<std::string> events;
    settings::State settings{};
    FakeStore store{events};
    FakeMessaging messaging{events};
    Harness ui{settings,messaging,store};

    // Incoming and enabled-session state are independent from UI selection.
    settings.communicator_view=View::SignalOnly;
    ui.begin();
    assert(ui.active_ && ui.selected_main_index_==kMainSignalIndex);
    ui.handle_main_input(side());
    assert(ui.selected_main_index_==kMainOptionsIndex);
    ui.handle_main_input(side());
    assert(ui.selected_main_index_==kMainReturnIndex);
    ui.handle_main_input(side());
    assert(ui.selected_main_index_==kMainSignalIndex);
    ui.handle_main_input(m5());
    assert(messaging.ring_calls==1 && messaging.preset_calls==0);
    assert(ui.ring_feedback==1);
    ui.handle_main_input(side());
    ui.handle_main_input(m5());
    assert(ui.options_active_ && ui.selected_options_index_==0);
    ui.handle_options_input(side());
    assert(ui.selected_options_index_==1);
    ui.handle_options_input(side());
    assert(ui.selected_options_index_==2);
    ui.handle_options_input(m5());
    assert(!ui.options_active_);
    ui.handle_main_input(side()); // POWROT from OPCJE.
    ui.handle_main_input(m5());
    assert(ui.foreground_exit_requested_);
    ui.foreground_exit_requested_=false;
    ui.begin();
    assert(ui.selected_main_index_==kMainSignalIndex);
    ui.reset_session();
    ui.begin();
    assert(ui.selected_main_index_==kMainSignalIndex);
    assert(store.radio_saves==0 && messaging.mode_calls==0);

    // Actual incoming screen handlers retain their response/close paths.
    ui.state_=Harness::State::IncomingPreset;
    ui.handle_incoming_preset_input(m5());
    assert(ui.state_==Harness::State::ChoosingResponse);
    ui.handle_response_choice_input(m5());
    assert(ui.fake_responses==1);
    ui.state_=Harness::State::IncomingPreset;
    ui.selected_main_index_=kMainOptionsIndex;
    ui.handle_incoming_preset_input(side());
    assert(ui.state_==Harness::State::Main
        && ui.selected_main_index_==kMainSignalIndex);
    ui.state_=Harness::State::IncomingResponse;
    ui.selected_main_index_=kMainReturnIndex;
    ui.handle_incoming_response_input(m5());
    assert(ui.state_==Harness::State::Main
        && ui.selected_main_index_==kMainSignalIndex);
    ui.state_=Harness::State::WaitDecision;
    ui.selected_main_index_=kMainReturnIndex;
    ui.selected_response_index_=1;
    ui.handle_wait_decision_input(m5());
    assert(ui.state_==Harness::State::Main
        && ui.selected_main_index_==kMainSignalIndex);

    // Production options view handler, including successful and failed save.
    ui.options_active_=true;
    ui.selected_options_index_=0;
    settings.communicator_view=View::List;
    store.view=0;
    ui.handle_options_input(m5());
    assert(settings.communicator_view==View::Single && store.view==1);
    ui.handle_options_input(m5());
    assert(settings.communicator_view==View::SignalOnly && store.view==2
        && ui.selected_main_index_==kMainSignalIndex);
    ui.handle_options_input(m5());
    assert(settings.communicator_view==View::List && store.view==0);
    store.fail_view=true;
    ui.handle_options_input(m5());
    assert(settings.communicator_view==View::List
        && store.view==0 && ui.preference_save_failed_);
    store.fail_view=false;
    ui.handle_options_input(m5());
    assert(settings.communicator_view==View::Single && store.view==1);

    // A successful radio change must persist before touching the radio.
    ui.selected_options_index_=1;
    events.clear(); messaging.mode_calls=0; store.radio_saves=0;
    ui.handle_options_input(m5());
    assert((events==std::vector<std::string>{"nvs_radio","radio"}));
    assert(store.mode==1 && messaging.mode==Mode::Lr
        && !ui.radio_mode_change_failed_ && !ui.preference_save_failed_
        && !ui.radio_preference_inconsistent_);

    // Reverse LR -> STANDARD is also durable and correctly ordered.
    events.clear();
    ui.handle_options_input(m5());
    assert((events==std::vector<std::string>{"nvs_radio","radio"}));
    assert(store.mode==0 && messaging.mode==Mode::Normal);

    // Failed NVS save must not touch radio or peer/discovery state.
    events.clear(); messaging.mode_calls=0;
    store.radio_saves=0; store.fail_radio_at=1;
    messaging.peer=true; messaging.discovery=true;
    ui.handle_options_input(m5());
    assert((events==std::vector<std::string>{"nvs_radio"}));
    assert(messaging.mode_calls==0 && messaging.mode==Mode::Normal);
    assert(messaging.peer && messaging.discovery && store.mode==0);
    assert(ui.preference_save_failed_ && !ui.radio_mode_change_failed_);
    assert(ui.footer=="NIE ZAPISANO");

    // Hardware failure AFTER NVS commit compensates with one bounded write.
    store.fail_radio_at=0; store.radio_saves=0;
    messaging.mode_calls=0; messaging.fail_mode_at=1;
    events.clear();
    ui.handle_options_input(m5());
    assert((events==std::vector<std::string>{"nvs_radio","radio","nvs_radio"}));
    assert(store.radio_saves==2 && messaging.mode_calls==1);
    assert(messaging.mode==Mode::Normal && store.mode==0);
    assert(ui.radio_mode_change_failed_ && !ui.radio_preference_inconsistent_);

    // Hardware fail AND restoration failure: error must remain explicit.
    store.radio_saves=0; store.fail_radio_at=2;
    messaging.mode_calls=0; messaging.fail_mode_at=1;
    events.clear();
    ui.handle_options_input(m5());
    assert((events==std::vector<std::string>{"nvs_radio","radio","nvs_radio"}));
    assert(store.mode==1 && messaging.mode==Mode::Normal);
    assert(ui.radio_mode_change_failed_ && ui.preference_save_failed_
        && ui.radio_preference_inconsistent_);
    assert(ui.footer=="RADIO/NVS NIESPOJNE");
    ui.handle_options_input(side()); // Navigating cannot silently erase warning.
    assert(ui.radio_preference_inconsistent_);
    assert(ui.footer=="RADIO/NVS NIESPOJNE");

    // A later failed hardware transition with successful compensation
    // reconciles an older persisted mismatch; clear the stale warning.
    ui.selected_options_index_=1;
    store.fail_radio_at=0; store.radio_saves=0;
    messaging.mode_calls=0; messaging.fail_mode_at=1;
    events.clear();
    ui.handle_options_input(m5());
    assert((events==std::vector<std::string>{"nvs_radio","radio","nvs_radio"}));
    assert(messaging.mode==Mode::Normal && store.mode==0);
    assert(ui.radio_mode_change_failed_ && !ui.radio_preference_inconsistent_);
    assert(ui.footer=="NIE UDALO SIE");

    // Explicit user retry succeeds, restoring consistent effective/durable mode.
    ui.selected_options_index_=1;
    store.fail_radio_at=0; store.radio_saves=0;
    messaging.mode_calls=0; messaging.fail_mode_at=0;
    events.clear();
    ui.handle_options_input(m5());
    assert(messaging.mode==Mode::Lr && store.mode==1
        && !ui.radio_preference_inconsistent_);
    assert(ui.footer=="OK");
    return 0;
}
'''
# Ensure control boundaries remain aligned with existing runtime.
assert "communicator_preferences.load_view()" in main
assert "communicator_preferences.load_radio_mode()" in main
assert "make_messaging_config(" in main
assert "communicator.begin();" in main
assert "communicator.reset_session();" in main
assert "begin_charging_incoming" in main and "DisplayOff" in main
assert "bool CommunicatorApp::process_incoming()" in src
assert "bool CommunicatorApp::process_incoming_for_charging()" in src
assert "void CommunicatorApp::start_signal_alert()" in src
assert "bool CommunicatorApp::send_selected_response()" in src

with tempfile.TemporaryDirectory() as directory:
    file = Path(directory) / "handlers.cpp"
    binary = Path(directory) / "handlers"
    file.write_text(cpp_head + "\n" + methods + "\n" + cpp_tail, encoding="utf-8")
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror",
                    str(file),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print("Communicator v0.3 R1 executed production handlers + failure order PASS")
