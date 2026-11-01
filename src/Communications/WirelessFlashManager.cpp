#include "WirelessFlashManager.hpp"

#if CONFIG_DEV_WIRELESS_FLASH

#include <StringUtil/StringUtil.hpp>
#include <esp_random.h>
#include <esp_timer.h>

#include "Communications/WiFi.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/PowerManager.hpp"
#include "WirelessFlashKey.h"
#include "sindarin-debug.h"

namespace {
constexpr int64_t CHALLENGE_LIFETIME_US = 30 * 1000 * 1000;
constexpr size_t NONCE_BYTES = 16;
constexpr int REBOOT_DELAY_MS = 1500;
} // namespace

auto WirelessFlashManager::getInstance() -> WirelessFlashManager & {
    static WirelessFlashManager instance;
    return instance;
}

auto WirelessFlashManager::start(int idleTimeoutMs, std::string *error) -> std::string {
    std::lock_guard<std::mutex> lock(mutex_);

    // Other features (e.g. sync) turn WiFi off when they finish, which strands the server.
    if (server_ != nullptr && !WiFi::getInstance().connected()) {
        stopLocked();
    }

    if (server_ == nullptr) {
        if (!WiFi::getInstance().hasWiFiCredentials()) {
            *error = "No WiFi credentials";
            return "";
        }
        if (!WiFi::getInstance().connectWiFiFromSettings()) {
            *error = "WiFi connection failed";
            return "";
        }

        auto server = std::make_unique<WirelessFlashServer>(
            WIRELESS_FLASH_KEY, [this](const WirelessFlashEvent &event) { handleEvent(event); });
        if (server->start() != ESP_OK) {
            WiFi::getInstance().disconnectWiFi();
            *error = "Failed to start server";
            return "";
        }
        server_ = std::move(server);
        address_ = StringFormat("%s:%u", WiFi::getInstance().getLocalIpStr().c_str(),
                                WirelessFlashServer::DEFAULT_PORT);
        idleTimeoutMs_ = idleTimeoutMs;
        log_i("Wireless flash listening at %s", address_.c_str());
    } else if (idleTimeoutMs == 0 || idleTimeoutMs_ != 0) {
        // An explicit (screen) start makes the session persistent; otherwise extend the window.
        idleTimeoutMs_ = idleTimeoutMs;
    }

    if (idleTimeoutMs_ > 0) {
        armIdleTimer();
    } else {
        cancelIdleTimer();
    }
    return address_;
}

void WirelessFlashManager::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    stopLocked();
}

void WirelessFlashManager::stopLocked() {
    cancelIdleTimer();
    if (server_ == nullptr) {
        return;
    }
    log_i("Wireless flash stopped");
    server_.reset();
    address_.clear();
    WiFi::getInstance().disconnectWiFi();
}

void WirelessFlashManager::stopIfIdle() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (server_ != nullptr && server_->isBusy()) {
        armIdleTimer();
        return;
    }
    log_i("Wireless flash idle timeout");
    stopLocked();
}

void WirelessFlashManager::armIdleTimer() {
    if (idleTimer_ == nullptr) {
        esp_timer_create_args_t args = {};
        args.callback = [](void * /*arg*/) {
            // Stopping WiFi blocks, which isn't allowed on the esp_timer task.
            DispatchQueue::dispatchOneOff([] { getInstance().stopIfIdle(); }, 4096, 1, 0,
                                          "WFlashIdle");
        };
        args.name = "wflash_idle";
        esp_timer_handle_t timer = nullptr;
        if (esp_timer_create(&args, &timer) != ESP_OK) {
            log_e("Failed to create wireless flash idle timer");
            return;
        }
        idleTimer_ = timer;
    }
    auto *timer = static_cast<esp_timer_handle_t>(idleTimer_);
    esp_timer_stop(timer);
    esp_timer_start_once(timer, static_cast<uint64_t>(idleTimeoutMs_.load()) * 1000);
}

void WirelessFlashManager::cancelIdleTimer() {
    if (idleTimer_ != nullptr) {
        esp_timer_stop(static_cast<esp_timer_handle_t>(idleTimer_));
    }
}

void WirelessFlashManager::setEventCallback(WirelessFlashServer::EventCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callback_ = std::move(callback);
}

void WirelessFlashManager::handleEvent(const WirelessFlashEvent &event) {
    // Runs on the HTTP server task, so avoid mutex_ (stop() holds it while joining that task).
    PowerManager::getInstance().feedInactivityTimer();

    switch (event.state) {
    case WirelessFlashEvent::State::Receiving:
        cancelIdleTimer();
        break;
    case WirelessFlashEvent::State::Failed:
        if (idleTimeoutMs_ > 0) {
            armIdleTimer();
        }
        break;
    case WirelessFlashEvent::State::Installed:
        // Reboot off the server task so the HTTP response is flushed first.
        DispatchQueue::dispatchOneOff(
            [] {
            vTaskDelay(pdMS_TO_TICKS(REBOOT_DELAY_MS));
            PowerManager::getInstance().reboot();
            },
            4096, 1, 0, "WFlashReboot");
        break;
    }

    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (callback_) {
        callback_(event);
    }
}

auto WirelessFlashManager::createChallenge() -> std::string {
    uint8_t bytes[NONCE_BYTES];
    esp_fill_random(bytes, sizeof(bytes));
    std::string nonce;
    for (uint8_t byte : bytes) {
        nonce += StringFormat("%02x", byte);
    }

    std::lock_guard<std::mutex> lock(challengeMutex_);
    nonce_ = nonce;
    nonceCreatedUs_ = esp_timer_get_time();
    return nonce;
}

auto WirelessFlashManager::verifyChallenge(const std::string &signature) -> bool {
    std::string nonce;
    {
        std::lock_guard<std::mutex> lock(challengeMutex_);
        nonce = std::move(nonce_);
        nonce_.clear();
        if (nonce.empty() || esp_timer_get_time() - nonceCreatedUs_ > CHALLENGE_LIFETIME_US) {
            return false;
        }
    }
    return wirelessFlashSignatureMatches(
        wirelessFlashSign(WIRELESS_FLASH_KEY, "wireless-flash:" + nonce), signature);
}

#endif
