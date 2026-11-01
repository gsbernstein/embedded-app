#pragma once

#include <atomic>
#include <cstdint>
#include <esp_err.h>
#include <functional>
#include <string>
#include <utility>
#include <vector>
struct httpd_req;

// Development-only HTTP endpoint that accepts a firmware image pushed from a computer on the same
// network (see scripts/ota_push.py / `make ota`). Only compiled in when CONFIG_DEV_WIRELESS_FLASH.
struct WirelessFlashEvent {
    enum class State { Receiving, Installed, Failed };

    State state;
    int percentage = 0;
    std::string message;
};

// Hex HMAC-SHA256 of `message`, or "" on failure.
auto wirelessFlashSign(const std::string &key, const std::string &message) -> std::string;
// Constant-time comparison of a lowercase hex `expected` against a client-provided signature.
auto wirelessFlashSignatureMatches(const std::string &expected, const std::string &actual) -> bool;

class WirelessFlashServer {
public:
    static constexpr uint16_t DEFAULT_PORT = 80;

    using EventCallback = std::function<void(const WirelessFlashEvent &event)>;

    // Uploads must carry an X-Signature header with the HMAC-SHA256 of the image under one of
    // the paired `keys`.
    WirelessFlashServer(std::vector<std::string> keys, EventCallback callback)
        : keys_(std::move(keys)), callback_(std::move(callback)) {}
    WirelessFlashServer(const WirelessFlashServer &) = delete;
    auto operator=(const WirelessFlashServer &) -> WirelessFlashServer & = delete;
    ~WirelessFlashServer() { stop(); }

    auto start(uint16_t port = DEFAULT_PORT) -> esp_err_t;
    void stop();
    [[nodiscard]] auto isBusy() const -> bool { return busy_; }

private:
    void *server_ = nullptr;
    std::vector<std::string> keys_;
    EventCallback callback_;
    std::atomic<bool> busy_{false};

    static auto handleInfo(httpd_req *req) -> esp_err_t;
    static auto handleFlash(httpd_req *req) -> esp_err_t;
    auto receiveFirmware(httpd_req *req) -> esp_err_t;
    void notify(WirelessFlashEvent::State state, int percentage, const std::string &message = "");
};
