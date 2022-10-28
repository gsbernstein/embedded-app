#include "UserReadable.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const Placemark &src, JsonVariant dst) -> bool {
    dst["uuid"] = src.uuid;
    dst["placemark"] = src.placemark;

    return true;
}
