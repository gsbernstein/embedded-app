#pragma once

#include <atomic>
#include <cstdint>
#include <esp_err.h>
#include <functional>
#include <string>
#include <utility>
struct httpd_req;

// Development-only HTTP endpoint that accepts a firmware image pushed from a computer on the same
// network (see scripts/ota_push.py / `make ota`). Only compiled in when CONFIG_DEV_WIRELESS_FLASH.
struct WirelessFlashEvent {
    enum class State { Receiving, Installed, Failed };

    State state;
    int percentage = 0;
    std::string message;
};

class WirelessFlashServer {
public:
    static constexpr uint16_t DEFAULT_PORT = 80;

    using EventCallback = std::function<void(const WirelessFlashEvent &event)>;

    explicit WirelessFlashServer(EventCallback callback) : callback_(std::move(callback)) {}
    WirelessFlashServer(const WirelessFlashServer &) = delete;
    auto operator=(const WirelessFlashServer &) -> WirelessFlashServer & = delete;
    ~WirelessFlashServer() { stop(); }

    auto start(uint16_t port = DEFAULT_PORT) -> esp_err_t;
    void stop();

private:
    void *server_ = nullptr;
    EventCallback callback_;
    std::atomic<bool> busy_{false};

    static auto handleInfo(httpd_req *req) -> esp_err_t;
    static auto handleFlash(httpd_req *req) -> esp_err_t;
    auto receiveFirmware(httpd_req *req) -> esp_err_t;
    void notify(WirelessFlashEvent::State state, int percentage, const std::string &message = "");
};
