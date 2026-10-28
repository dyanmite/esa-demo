#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "sos_controller.h"
#include "medicine_controller.h"
#include <ssid_manager.h>

#include <esp_log.h>

#define TAG "CompactWifiBoard"

class CompactWifiBoard : public WifiBoard {
private:
    Display* display_ = nullptr;
    Button boot_button_;
    Button touch_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    Button sos_button_;

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        touch_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        touch_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });

        sos_button_.OnClick([this]() {
            ESP_LOGW(TAG, "Hardware SOS Button Pressed!");
            auto codec = static_cast<NoAudioCodecSimplex*>(GetAudioCodec());
            codec->TestSpeakerTone(880, 200);
            codec->TestSpeakerTone(1760, 200);
            SosController::GetInstance().TriggerAlert("Physical breadboard SOS button was pressed!");
        });
    }

    void InitializeTools() {
        MedicineController::GetInstance().Initialize();

        // 1. SOS Emergency Alert
        McpServer::GetInstance().AddTool(
            "sos.trigger_emergency",
            "Trigger emergency alert to caretaker if elderly user needs help, feels sick, or had a fall.",
            PropertyList({Property("details", kPropertyTypeString, "Details of emergency")}),
            [](const PropertyList& properties) -> ReturnValue {
                std::string details = "Emergency: Voice Assistant triggered";
                try {
                    details = properties["details"].value<std::string>();
                } catch (...) {
                }
                SosController::GetInstance().TriggerAlert(details);
                return "Emergency SOS alert has been sent to caretaker.";
            });

        // 2. Add or update medicine schedule
        McpServer::GetInstance().AddTool(
            "medicine.add_schedule",
            "Add or update a medicine reminder schedule for the elderly user. Call when user asks to schedule, remember, or add a medicine.",
            PropertyList({
                Property("medicine_name", kPropertyTypeString),
                Property("time", kPropertyTypeString),
                Property("dosage", kPropertyTypeString, std::string("1 dose")),
                Property("instructions", kPropertyTypeString, std::string(""))
            }),
            [](const PropertyList& properties) -> ReturnValue {
                std::string name = "Medicine";
                std::string time = "09:00 AM";
                std::string dosage = "1 dose";
                std::string instructions = "";
                try { name = properties["medicine_name"].value<std::string>(); } catch (...) {}
                try { time = properties["time"].value<std::string>(); } catch (...) {}
                try { dosage = properties["dosage"].value<std::string>(); } catch (...) {}
                try { instructions = properties["instructions"].value<std::string>(); } catch (...) {}

                return MedicineController::GetInstance().AddSchedule(name, time, dosage, instructions);
            });

        // 3. Get medicine schedule
        McpServer::GetInstance().AddTool(
            "medicine.get_schedule",
            "Get the list of all scheduled medicines for the elderly user. Call when user asks what medicines they have, their medicine schedule, or what pills they need to take.",
            PropertyList(),
            [](const PropertyList& properties) -> ReturnValue {
                return MedicineController::GetInstance().GetSchedule();
            });

        // 4. Mark medicine as taken
        McpServer::GetInstance().AddTool(
            "medicine.mark_taken",
            "Record that the elderly user has taken their medicine and notify caretaker. Call when user says they took their medicine or pills.",
            PropertyList({
                Property("medicine_name", kPropertyTypeString)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                std::string name = "Medicine";
                try { name = properties["medicine_name"].value<std::string>(); } catch (...) {}
                return MedicineController::GetInstance().MarkTaken(name);
            });

        // 5. Remove medicine schedule
        McpServer::GetInstance().AddTool(
            "medicine.remove_schedule",
            "Remove a medicine from the schedule. Call when user asks to delete, cancel, or remove a medicine reminder.",
            PropertyList({
                Property("medicine_name", kPropertyTypeString)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                std::string name = "Medicine";
                try { name = properties["medicine_name"].value<std::string>(); } catch (...) {}
                return MedicineController::GetInstance().RemoveSchedule(name);
            });
    }

public:
    CompactWifiBoard() :
        boot_button_(BOOT_BUTTON_GPIO),
        touch_button_(TOUCH_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO),
        sos_button_(SOS_BUTTON_GPIO) {
        // No OLED display fitted: skip I2C & SSD1306 init to avoid boot crash
        display_ = new NoDisplay();

        auto& ssid_manager = SsidManager::GetInstance();
        ssid_manager.AddSsid("AndroidAP_4175", "jash46666");
        ssid_manager.AddSsid("iPhone", "tejareddy");

        InitializeButtons();
        InitializeTools();
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, I2S_STD_SLOT_BOTH,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN, I2S_STD_SLOT_LEFT);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(CompactWifiBoard);
