#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Communications/WirelessFlashServer.hpp"

class DirectCommand;

// Owns the WiFi + HTTP server lifecycle for development wireless flashing. Started either from
// Debug > Wireless Flash (stays up until stopped) or remotely over BLE after a challenge-response
// (stays up for a short idle window). Reboots into the new image once a signed push is installed.
// Computers are authorized by pairing a key over USB serial (`make pair`), stored in NVS.
class WirelessFlashManager {
public:
    static auto getInstance() -> WirelessFlashManager &;

    WirelessFlashManager() = default;
    WirelessFlashManager(const WirelessFlashManager &) = delete;
    auto operator=(const WirelessFlashManager &) -> WirelessFlashManager & = delete;

    // Blocks while WiFi connects. `idleTimeoutMs` of 0 keeps listening until stop(). Returns
    // "ip:port" on success; on failure returns "" and sets `error`.
    auto start(int idleTimeoutMs, std::string *error) -> std::string;
    void stop();

    // Receives server events (on the HTTP server task). Pass nullptr to clear.
    void setEventCallback(WirelessFlashServer::EventCallback callback);

    // BLE trigger: hand out a single-use nonce, then accept HMAC(key, "wireless-flash:" + nonce)
    // for any paired key.
    auto createChallenge() -> std::string;
    auto verifyChallenge(const std::string &signature) -> bool;

    static constexpr size_t MAX_PAIRED_KEYS = 4;

    // Adds or replaces the key for `label`. Returns false and sets `error` if invalid or full.
    auto pair(const std::string &label, const std::string &hexKey, std::string *error) -> bool;
    // Removes the key for `label`, or every key if `label` is empty. Returns how many were removed.
    auto unpair(const std::string &label) -> size_t;
    auto pairedLabels() -> std::vector<std::string>;

    // Registers PAIR / UNPAIR / PAIRED commands on the USB serial control port.
    void registerCommands(DirectCommand &directCommand);

private:
    std::mutex mutex_;
    std::unique_ptr<WirelessFlashServer> server_;
    std::string address_;
    void *idleTimer_ = nullptr;
    std::atomic<int> idleTimeoutMs_{0};

    std::mutex callbackMutex_;
    WirelessFlashServer::EventCallback callback_;

    struct PairedKey {
        std::string label;
        std::string key;
    };
    std::mutex keysMutex_;
    auto loadKeysLocked() -> std::vector<PairedKey>;
    void saveKeysLocked(const std::vector<PairedKey> &keys);
    auto pairedKeys() -> std::vector<std::string>;

    std::mutex challengeMutex_;
    std::string nonce_;
    int64_t nonceCreatedUs_ = 0;

    void handleEvent(const WirelessFlashEvent &event);
    void armIdleTimer();
    void cancelIdleTimer();
    void stopIfIdle();
    void stopLocked();
};
