#include <string>

#include "Models/Firmware.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/Placemark.hpp"

class CheckSyncResponse : JsonSerializable {
public:
    CheckSyncResponse() = default;
    ~CheckSyncResponse() override = default;

    bool synced;
    std::string message;
    Firmware firmware;
    std::vector<Placemark> placemarks;

    auto fromJson(const JsonObject &j) -> bool override {
        synced = j["synced"].as<bool>();
        message = j["message"].as<std::string>();
        firmware.fromJson(j["firmware"]);
        placemarks.clear();
        for (const auto &placemark : j["placemarks"].as<JsonArray>()) {
            placemarks.emplace_back();
            placemarks.back().fromJson(placemark);
        }
        return true;
    }
};
