# 🚀 Smart Elderly Assistant (ESA) — Complete Changelog & Setup Guide

This guide documents **all modifications made since yesterday** and provides **step-by-step instructions** for building, flashing, and running the project on an ESP32-S3 board with ESP-IDF.

---

## 📌 1. Summary of What Was Changed Since Yesterday

| Subsystem | What Changed | Why It Was Needed |
| :--- | :--- | :--- |
| **I2S Audio Output (Speaker)** | Fixed `.left_align = true` in `main/audio/codecs/no_audio_codec.cc` | On ESP32-S3 (`I2S_HW_VERSION_2`), setting `.left_align = false` right-aligned the 32-bit slot, placing audio MSBs at the end. MAX98357A received zeros and truncation noise, causing loud digital static. Restoring `true` gives crystal-clear audio. |
| **Audio Sample Rate** | Aligned `AUDIO_OUTPUT_SAMPLE_RATE` to `16000` in `main/boards/bread-compact-wifi/config.h` | The cloud server sends Opus frames at 16 kHz. Setting output to 16 kHz removes the `16000 -> 24000` software resampler, eliminating audio latency and artifacts. |
| **No-Display Boot Stability** | Initialized `NoDisplay()` in `main/boards/bread-compact-wifi/compact_wifi_board.cc` | Avoids I2C bus scan timeouts or kernel crashes if an SSD1306/SH1106 OLED is not physically plugged in. |
| **Medicine Schedule Controller** | Added `main/medicine_controller.h` and `main/medicine_controller.cc` | Full voice-controlled medicine scheduling system with Flash (NVS) persistence and automatic Telegram caretaker notifications. |
| **Medicine MCP Tools** | Added 4 voice tools: `medicine.add_schedule`, `medicine.get_schedule`, `medicine.mark_taken`, `medicine.remove_schedule` | Allows the elderly user to converse with the AI naturally to schedule pills, check what medicines to take, and record doses. |
| **Telegram Notifications** | Added `SendNotification()` to `main/sos_controller.cc` & `main/sos_controller.h` | Dispatches real-time Telegram alerts to the caretaker for both emergencies and medicine adherence without blocking audio tasks. |
| **Hardware SOS Button** | Wired to GPIO 7 with dual-tone audio chime (880 Hz + 1760 Hz) | Dedicated physical push button for immediate help. Also monitors voice keywords ("help", "fall down", "emergency", "doctor"). |

---

## 🔌 2. Hardware Wiring & Pinout Reference

Make sure all connections match the following pin layout:

### A. MAX98357A Class-D I2S Amplifier & Speaker (Output)
| MAX98357A Pin | ESP32-S3 Pin | Notes |
| :--- | :--- | :--- |
| **VIN** | **5V** (or VBUS) | Provide clean 5V power for loud, crisp volume |
| **GND** | **GND** | Common ground with ESP32-S3 |
| **BCLK** | **GPIO 9** | Bit Clock |
| **LRC (WS)** | **GPIO 10** | Word Select / Left-Right Clock |
| **DIN** | **GPIO 8** | Serial Audio Data Output |
| **GAIN** | *Leave Floating* | Default +9 dB gain (recommended for 3W speaker) |
| **SD_MODE** | **3.3V** or floating | Pull to 3.3V or leave unconnected for Left/Mixed audio |

### B. INMP441 MEMS Microphone (Input)
| INMP441 Pin | ESP32-S3 Pin | Notes |
| :--- | :--- | :--- |
| **VDD** | **3.3V** | Do NOT connect to 5V |
| **GND** | **GND** | Common ground |
| **SCK** | **GPIO 5** | Serial Clock |
| **WS** | **GPIO 4** | Word Select |
| **SD** | **GPIO 6** | Serial Data Input |
| **L/R** | **GND** | Ties microphone to Left channel |

### C. Buttons & Indicators
| Component | ESP32-S3 Pin | Notes |
| :--- | :--- | :--- |
| **SOS Push Button** | **GPIO 7** to **GND** | Internal pull-up enabled. Pressing sends Telegram alert and plays alarm tone |
| **BOOT Button** | **GPIO 0** | Built-in button; click to toggle listening state |
| **Built-in LED** | **GPIO 48** | Status indicator |

---

## 💻 3. Step-by-Step Instructions for Your Friend

Your friend already has ESP-IDF installed. They can set up and run the code with these exact steps:

### Step 1: Clone or Pull the Latest Code
```bash
git clone https://github.com/dyanmite/esa-demo.git
cd esa-demo
```
*(Or if they already have the folder cloned: `git pull origin main`)*

### Step 2: Open Terminal & Activate ESP-IDF Environment
Depending on their operating system:

- **On Windows (PowerShell with ESP-IDF 5.5 / 6.0)**:
  ```powershell
  . C:\Espressif\frameworks\esp-idf-v5.5.6\export.ps1
  # Or use their ESP-IDF PowerShell Shortcut from Start Menu
  ```
- **On Linux / macOS**:
  ```bash
  . $HOME/esp/esp-idf/export.sh
  ```

Check that the toolchain is active:
```bash
idf.py --version
```

### Step 3: Check Wi-Fi Credentials (Optional)
In `main/boards/bread-compact-wifi/compact_wifi_board.cc` (lines 109-111), verify or add your friend's local Wi-Fi SSID and password:
```cpp
auto& ssid_manager = SsidManager::GetInstance();
ssid_manager.AddSsid("YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD");
```

### Step 4: Build the Project
Run:
```bash
idf.py build
```
*(This will compile `medicine_controller.cc`, `sos_controller.cc`, `no_audio_codec.cc`, and generate `build/xiaozhi.bin` with zero errors).*

### Step 5: Flash and Monitor
Connect the ESP32-S3 board via USB. Find the COM port (e.g. `COM12` on Windows, or `/dev/ttyUSB0` on Linux):

```bash
idf.py -p COM12 flash monitor
```

*(To exit the monitor at any time, press `Ctrl + ]`).*

---

## 🎯 4. How to Test All Features

### Test 1: Voice AI Conversation & Audio Output
1. Once connected to Wi-Fi, the board logs `State: idle`.
2. Speak: **"Hello XiaoZhi"** or press the **BOOT button** (GPIO 0).
3. Say: *"Hello, how are you today?"*
4. Listen to the AI reply through the speaker. Verify the voice is clear, crisp, and has **zero static or digital noise**.

### Test 2: Voice-Driven Medicine Scheduling
Speak to the AI using any of these commands:

- **Check Current Schedule**:
  > *"What are my medicines for today?"*  
  > **AI will reply**: Reads out your scheduled medicines (preloaded with *"Blood Pressure Pill at 09:00 AM"*).

- **Add a New Medicine**:
  > *"Remind me to take Metformin 500mg at 8 PM after dinner."*  
  > **AI will reply**: *"I have scheduled Metformin for 8:00 PM (500mg). Your caretaker has been informed."*  
  > *(The schedule is saved to flash memory and sent to the Telegram bot).*

- **Mark Medicine as Taken**:
  > *"I took my blood pressure pill."*  
  > **AI will reply**: *"Wonderful! I have recorded that you took your Blood Pressure Pill, and your caretaker has been updated."*

- **Remove a Medicine**:
  > *"Remove my Aspirin reminder."*  
  > **AI will reply**: Confirms removal and updates flash.

### Test 3: Emergency SOS Alert
- **Button Press**: Press the push button connected between **GPIO 7 and GND**. The speaker will play an 880 Hz + 1760 Hz alert tone, and an `EMERGENCY_ALERT_FOR_ELDERLY` message will arrive on the caretaker's Telegram bot.
- **Voice Trigger**: Say into the microphone: *"Help, I fell down!"* or *"I need a doctor!"*. The system detects the emergency keywords and immediately dispatches the Telegram alert.

---

## 📱 Telegram Bot Setup (Already Configured)
The project is pre-configured with:
- **Bot Token**: `8860203910:AAE0PgoKdzmcHJ5xHJj32VKbE-IM3YYSdFE`
- **Chat ID**: `6125494215`

Caretakers receive live alerts whenever:
1. Physical SOS button is pressed.
2. Emergency voice keywords are detected.
3. Medicine schedule is added or updated.
4. Elderly logs that they took their prescribed medicine.
