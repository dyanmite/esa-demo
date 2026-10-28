#include "medicine_controller.h"
#include "sos_controller.h"
#include "settings.h"

#include <esp_log.h>
#include <cJSON.h>
#include <algorithm>
#include <cctype>

#define TAG "MedicineController"

static bool CaseInsensitiveEqual(const std::string& a, const std::string& b) {
    if (a.length() != b.length()) return false;
    for (size_t i = 0; i < a.length(); ++i) {
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) {
            return false;
        }
    }
    return true;
}

static bool CaseInsensitiveContains(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char ch1, char ch2) { return std::tolower((unsigned char)ch1) == std::tolower((unsigned char)ch2); }
    );
    return (it != haystack.end());
}

void MedicineController::Initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return;
    LoadFromNvs();
    initialized_ = true;
    ESP_LOGI(TAG, "MedicineController initialized with %d schedule(s)", (int)items_.size());
}

void MedicineController::LoadFromNvs() {
    Settings settings("medicine", false);
    std::string json_str = settings.GetString("schedules", "");
    if (json_str.empty()) {
        // Pre-populate with a demo reminder for elderly care
        items_.push_back({
            "Blood Pressure Pill",
            "09:00 AM",
            "1 tablet",
            "Take after breakfast with water"
        });
        SaveToNvs();
        return;
    }

    cJSON* root = cJSON_Parse(json_str.c_str());
    if (root == nullptr || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return;
    }

    items_.clear();
    int count = cJSON_GetArraySize(root);
    for (int i = 0; i < count; ++i) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        if (cJSON_IsObject(item)) {
            MedicineItem med;
            cJSON* n = cJSON_GetObjectItem(item, "name");
            cJSON* t = cJSON_GetObjectItem(item, "time");
            cJSON* d = cJSON_GetObjectItem(item, "dosage");
            cJSON* in = cJSON_GetObjectItem(item, "instructions");
            if (n && n->valuestring) med.name = n->valuestring;
            if (t && t->valuestring) med.time = t->valuestring;
            if (d && d->valuestring) med.dosage = d->valuestring;
            if (in && in->valuestring) med.instructions = in->valuestring;
            if (!med.name.empty()) {
                items_.push_back(med);
            }
        }
    }
    cJSON_Delete(root);
}

void MedicineController::SaveToNvs() {
    cJSON* root = cJSON_CreateArray();
    for (const auto& item : items_) {
        cJSON* obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "name", item.name.c_str());
        cJSON_AddStringToObject(obj, "time", item.time.c_str());
        cJSON_AddStringToObject(obj, "dosage", item.dosage.c_str());
        cJSON_AddStringToObject(obj, "instructions", item.instructions.c_str());
        cJSON_AddItemToArray(root, obj);
    }
    char* printed = cJSON_PrintUnformatted(root);
    if (printed) {
        Settings settings("medicine", true);
        settings.SetString("schedules", printed);
        cJSON_free(printed);
    }
    cJSON_Delete(root);
}

std::string MedicineController::AddSchedule(const std::string& name, const std::string& time,
                                            const std::string& dosage, const std::string& instructions) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool updated = false;
    for (auto& item : items_) {
        if (CaseInsensitiveEqual(item.name, name)) {
            item.time = time;
            if (!dosage.empty()) item.dosage = dosage;
            if (!instructions.empty()) item.instructions = instructions;
            updated = true;
            break;
        }
    }

    if (!updated) {
        items_.push_back({name, time, dosage.empty() ? "1 dose" : dosage, instructions});
    }

    SaveToNvs();

    std::string notify_msg = "Added medicine reminder: " + name + " at " + time + " (" + (dosage.empty() ? "1 dose" : dosage) + ")";
    SosController::GetInstance().SendNotification("MEDICINE_SCHEDULE", notify_msg);

    std::string result = "I have scheduled " + name + " for " + time + " (" + (dosage.empty() ? "1 dose" : dosage) + "). Your caretaker has been informed.";
    return result;
}

std::string MedicineController::GetSchedule() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (items_.empty()) {
        return "You currently have no medicines scheduled. You can ask me to add one anytime, like remind me to take blood pressure pill at 9 AM.";
    }

    std::string result = "You have " + std::to_string(items_.size()) + " medicine reminder" + (items_.size() > 1 ? "s" : "") + ": ";
    for (size_t i = 0; i < items_.size(); ++i) {
        result += std::to_string(i + 1) + ". " + items_[i].name + " at " + items_[i].time + " (" + items_[i].dosage;
        if (!items_[i].instructions.empty()) {
            result += ", " + items_[i].instructions;
        }
        result += "). ";
    }
    return result;
}

std::string MedicineController::MarkTaken(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string matched_name = name;

    if (!items_.empty()) {
        for (const auto& item : items_) {
            if (CaseInsensitiveContains(item.name, name) || CaseInsensitiveContains(name, item.name)) {
                matched_name = item.name;
                break;
            }
        }
        if (matched_name.empty() || CaseInsensitiveEqual(name, "all") || CaseInsensitiveEqual(name, "medicine")) {
            matched_name = items_[0].name;
        }
    }

    std::string notify_msg = "Elderly recorded taking medicine: " + matched_name;
    SosController::GetInstance().SendNotification("MEDICINE_TAKEN", notify_msg);

    return "Wonderful! I have recorded that you took your " + matched_name + ", and your caretaker has been updated.";
}

std::string MedicineController::RemoveSchedule(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::remove_if(items_.begin(), items_.end(), [&](const MedicineItem& item) {
        return CaseInsensitiveContains(item.name, name) || CaseInsensitiveContains(name, item.name);
    });

    if (it != items_.end()) {
        items_.erase(it, items_.end());
        SaveToNvs();
        SosController::GetInstance().SendNotification("MEDICINE_REMOVED", "Removed schedule for: " + name);
        return "I have removed " + name + " from your medicine schedule.";
    }

    return "I couldn't find a medicine named " + name + " in your schedule.";
}
