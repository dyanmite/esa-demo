#ifndef _MEDICINE_CONTROLLER_H_
#define _MEDICINE_CONTROLLER_H_

#include <string>
#include <vector>
#include <mutex>

struct MedicineItem {
    std::string name;
    std::string time;
    std::string dosage;
    std::string instructions;
};

class MedicineController {
public:
    static MedicineController& GetInstance() {
        static MedicineController instance;
        return instance;
    }

    void Initialize();
    std::string AddSchedule(const std::string& name, const std::string& time, 
                           const std::string& dosage = "1 dose", 
                           const std::string& instructions = "");
    std::string GetSchedule();
    std::string MarkTaken(const std::string& name);
    std::string RemoveSchedule(const std::string& name);

private:
    MedicineController() = default;
    void LoadFromNvs();
    void SaveToNvs();

    std::vector<MedicineItem> items_;
    std::mutex mutex_;
    bool initialized_ = false;
};

#endif // _MEDICINE_CONTROLLER_H_
