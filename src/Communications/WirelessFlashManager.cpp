#include "WirelessFlashManager.hpp"

#if CONFIG_DEV_WIRELESS_FLASH

#include <StringUtil/StringUtil.hpp>
#include <algorithm>
#include <cctype>
#include <esp_random.h>
#include <esp_timer.h>
#include <sstream>

#include "Communications/WiFi.hpp"
#include "Misc/DirectCommand.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/PowerManager.hpp"
#include "Storage/PreferencesStore.hpp"
#include "sindarin-debug.h"

extern PreferencesStore prefs;

namespace {
constexpr int64_t CHALLENGE_LIFETIME_US = 30 * 1000 * 1000;
constexpr size_t NONCE_BYTES = 16;
constexpr int REBOOT_DELAY_MS = 1500;
constexpr size_t MAX_LABEL_LENGTH = 32;
constexpr size_t MIN_KEY_HEX_LENGTH = 32;
constexpr size_t MAX_KEY_HEX_LENGTH = 128;

auto isValidLabel(const std::string &label) -> bool {
    return !label.empty() && label.size() <= MAX_LABEL_LENGTH &&
           std::all_of(label.begin(), label.end(), [](unsigned char c) {
               return std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '@';
           });
}

auto isValidKey(const std::string &key) -> bool {
    return key.size() >= MIN_KEY_HEX_LENGTH && key.size() <= MAX_KEY_HEX_LENGTH &&
           key.size() % 2 == 0 &&
           std::all_of(key.begin(), key.end(), [](unsigned char c) { return std::isxdigit(c); });
}
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
        auto keys = pairedKeys();
        if (keys.empty()) {
            *error = "Not paired (run make pair)";
            return "";
        }
        if (!WiFi::getInstance().hasWiFiCredentials()) {
            *error = "No WiFi credentials";
            return "";
        }
        if (!WiFi::getInstance().connectWiFiFromSettings()) {
            *error = "WiFi connection failed";
            return "";
        }

        auto server = std::make_unique<WirelessFlashServer>(
            std::move(keys), [this](const WirelessFlashEvent &event) { handleEvent(event); });
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
    bool valid = false;
    for (const auto &key : pairedKeys()) {
        valid |= wirelessFlashSignatureMatches(wirelessFlashSign(key, "wireless-flash:" + nonce),
                                               signature);
    }
    return valid;
}

auto WirelessFlashManager::loadKeysLocked() -> std::vector<PairedKey> {
    std::vector<PairedKey> keys;
    std::istringstream lines(prefs.wirelessFlashKeys.get().value_or(""));
    std::string line;
    while (std::getline(lines, line)) {
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            keys.push_back({line.substr(0, colon), line.substr(colon + 1)});
        }
    }
    return keys;
}

void WirelessFlashManager::saveKeysLocked(const std::vector<PairedKey> &keys) {
    if (keys.empty()) {
        prefs.wirelessFlashKeys.remove();
        return;
    }
    std::string value;
    for (const auto &entry : keys) {
        value += entry.label + ":" + entry.key + "\n";
    }
    prefs.wirelessFlashKeys.set(value);
}

auto WirelessFlashManager::pairedKeys() -> std::vector<std::string> {
    std::lock_guard<std::mutex> lock(keysMutex_);
    std::vector<std::string> result;
    for (auto &entry : loadKeysLocked()) {
        result.push_back(std::move(entry.key));
    }
    return result;
}

auto WirelessFlashManager::pair(const std::string &label, const std::string &hexKey,
                                std::string *error) -> bool {
    if (!isValidLabel(label)) {
        *error = "invalid label";
        return false;
    }
    if (!isValidKey(hexKey)) {
        *error = "key must be 32-128 hex chars";
        return false;
    }
    std::string key = hexKey;
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    std::lock_guard<std::mutex> lock(keysMutex_);
    auto keys = loadKeysLocked();
    auto existing = std::find_if(keys.begin(), keys.end(),
                                 [&label](const PairedKey &entry) { return entry.label == label; });
    if (existing != keys.end()) {
        existing->key = key;
    } else if (keys.size() >= MAX_PAIRED_KEYS) {
        *error = "too many paired keys (UNPAIR one)";
        return false;
    } else {
        keys.push_back({label, key});
    }
    saveKeysLocked(keys);
    log_i("Paired wireless flash key '%s'", label.c_str());
    return true;
}

auto WirelessFlashManager::unpair(const std::string &label) -> size_t {
    std::lock_guard<std::mutex> lock(keysMutex_);
    auto keys = loadKeysLocked();
    auto before = keys.size();
    if (label.empty()) {
        keys.clear();
    } else {
        keys.erase(
            std::remove_if(keys.begin(), keys.end(),
                           [&label](const PairedKey &entry) { return entry.label == label; }),
            keys.end());
    }
    saveKeysLocked(keys);
    log_i("Unpaired %zu wireless flash key(s)", before - keys.size());
    return before - keys.size();
}

auto WirelessFlashManager::pairedLabels() -> std::vector<std::string> {
    std::lock_guard<std::mutex> lock(keysMutex_);
    std::vector<std::string> labels;
    for (auto &entry : loadKeysLocked()) {
        labels.push_back(std::move(entry.label));
    }
    return labels;
}

void WirelessFlashManager::registerCommands(DirectCommand &directCommand) {
    directCommand.addCommandHandler([this](const std::string &command, const std::string &payload,
                                           const DirectCommand::TxLine &txLine) {
        if (command == "PAIR") {
            auto space = payload.find(' ');
            std::string error = "usage: PAIR <label> <hexkey>";
            if (space != std::string::npos &&
                pair(payload.substr(0, space), payload.substr(space + 1), &error)) {
                txLine("PAIR OK " + payload.substr(0, space));
            } else {
                txLine("PAIR ERROR " + error);
            }
            return true;
        }
        if (command == "UNPAIR") {
            txLine(StringFormat("UNPAIR OK %zu", unpair(payload)));
            return true;
        }
        if (command == "PAIRED") {
            std::string labels;
            for (const auto &label : pairedLabels()) {
                labels += (labels.empty() ? "" : ",") + label;
            }
            txLine("PAIRED " + labels);
            return true;
        }
        return false;
    });
}

#endif
