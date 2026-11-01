#include "WirelessFlashServer.hpp"

#if CONFIG_DEV_WIRELESS_FLASH

#include <StringUtil/StringUtil.hpp>
#include <Update.h>
#include <algorithm>
#include <cctype>
#include <esp_http_server.h>
#include <esp_ota_ops.h>
#include <mbedtls/md.h>
#include <memory>

#include "Version.hpp"
#include "sindarin-debug.h"

namespace {
constexpr size_t RECV_CHUNK_SIZE = 4096;
constexpr int MAX_RECV_TIMEOUTS = 3;
constexpr size_t MD5_HEX_LENGTH = 32;
constexpr size_t SIGNATURE_HEX_LENGTH = 64;

auto toHex(const uint8_t *bytes, size_t len) -> std::string {
    static constexpr char DIGITS[] = "0123456789abcdef";
    std::string hex(len * 2, '0');
    for (size_t i = 0; i < len; i++) {
        hex[i * 2] = DIGITS[bytes[i] >> 4];
        hex[i * 2 + 1] = DIGITS[bytes[i] & 0x0f];
    }
    return hex;
}

class HmacSha256 {
public:
    explicit HmacSha256(const std::string &key) {
        mbedtls_md_init(&ctx_);
        ok_ = mbedtls_md_setup(&ctx_, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1) == 0 &&
              mbedtls_md_hmac_starts(&ctx_, reinterpret_cast<const uint8_t *>(key.data()),
                                     key.size()) == 0;
    }
    HmacSha256(const HmacSha256 &) = delete;
    auto operator=(const HmacSha256 &) -> HmacSha256 & = delete;
    ~HmacSha256() { mbedtls_md_free(&ctx_); }

    void update(const uint8_t *data, size_t len) {
        ok_ = ok_ && mbedtls_md_hmac_update(&ctx_, data, len) == 0;
    }

    // Returns an empty string on failure so it never matches a signature.
    auto finishHex() -> std::string {
        uint8_t digest[32];
        if (!ok_ || mbedtls_md_hmac_finish(&ctx_, digest) != 0) {
            return "";
        }
        return toHex(digest, sizeof(digest));
    }

private:
    mbedtls_md_context_t ctx_{};
    bool ok_ = false;
};

auto sendStatus(httpd_req_t *req, const char *status, const std::string &body) -> esp_err_t {
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, body.c_str());
}
} // namespace

auto wirelessFlashSign(const std::string &key, const std::string &message) -> std::string {
    HmacSha256 hmac(key);
    hmac.update(reinterpret_cast<const uint8_t *>(message.data()), message.size());
    return hmac.finishHex();
}

auto wirelessFlashSignatureMatches(const std::string &expected, const std::string &actual) -> bool {
    if (expected.empty() || expected.size() != actual.size()) {
        return false;
    }
    uint8_t diff = 0;
    for (size_t i = 0; i < expected.size(); i++) {
        diff |= static_cast<uint8_t>(expected[i] ^ std::tolower(actual[i]));
    }
    return diff == 0;
}

auto WirelessFlashServer::start(uint16_t port) -> esp_err_t {
    if (server_ != nullptr) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.stack_size = 8192;
    config.recv_wait_timeout = 10;
    config.lru_purge_enable = true;

    httpd_handle_t server = nullptr;
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        log_e("httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }
    server_ = server;

    httpd_uri_t info = {
        .uri = "/info", .method = HTTP_GET, .handler = &handleInfo, .user_ctx = this};
    httpd_uri_t flash = {
        .uri = "/flash", .method = HTTP_POST, .handler = &handleFlash, .user_ctx = this};
    httpd_register_uri_handler(server, &info);
    httpd_register_uri_handler(server, &flash);

    log_i("Wireless flash server listening on port %u", port);
    return ESP_OK;
}

void WirelessFlashServer::stop() {
    if (server_ == nullptr) {
        return;
    }
    httpd_stop(static_cast<httpd_handle_t>(server_));
    server_ = nullptr;
    if (busy_) {
        Update.abort();
        busy_ = false;
    }
}

void WirelessFlashServer::notify(WirelessFlashEvent::State state, int percentage,
                                 const std::string &message) {
    if (callback_) {
        callback_(WirelessFlashEvent{.state = state, .percentage = percentage, .message = message});
    }
}

auto WirelessFlashServer::handleInfo(httpd_req_t *req) -> esp_err_t {
    const esp_partition_t *running = esp_ota_get_running_partition();
    auto body = StringFormat(R"({"version":"%s","partition":"%s"})", SemanticVersionString(),
                             running != nullptr ? running->label : "?");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, body.c_str());
}

auto WirelessFlashServer::handleFlash(httpd_req_t *req) -> esp_err_t {
    auto *self = static_cast<WirelessFlashServer *>(req->user_ctx);

    bool expected = false;
    if (!self->busy_.compare_exchange_strong(expected, true)) {
        return sendStatus(req, "409 Conflict", "Another flash is already in progress\n");
    }

    esp_err_t result = self->receiveFirmware(req);
    self->busy_ = false;
    return result;
}

auto WirelessFlashServer::receiveFirmware(httpd_req_t *req) -> esp_err_t {
    const size_t total = req->content_len;
    if (total == 0) {
        return sendStatus(req, "411 Length Required", "Missing Content-Length\n");
    }

    auto fail = [this, req](const std::string &message) -> esp_err_t {
        log_e("Wireless flash failed: %s", message.c_str());
        Update.abort();
        notify(WirelessFlashEvent::State::Failed, 0, message);
        return sendStatus(req, "500 Internal Server Error", message + "\n");
    };

    char signature[SIGNATURE_HEX_LENGTH + 1] = {};
    if (httpd_req_get_hdr_value_len(req, "X-Signature") != SIGNATURE_HEX_LENGTH ||
        httpd_req_get_hdr_value_str(req, "X-Signature", signature, sizeof(signature)) != ESP_OK) {
        log_w("Rejected wireless flash without a signature");
        return sendStatus(req, "401 Unauthorized", "Missing X-Signature\n");
    }

    log_i("Receiving firmware over WiFi (%u bytes)", static_cast<unsigned>(total));
    notify(WirelessFlashEvent::State::Receiving, 0);

    if (!Update.begin(total, U_FLASH)) {
        return fail(StringFormat("Update.begin: %s", Update.errorString()));
    }

    char md5[MD5_HEX_LENGTH + 1] = {};
    if (httpd_req_get_hdr_value_len(req, "X-MD5") == MD5_HEX_LENGTH &&
        httpd_req_get_hdr_value_str(req, "X-MD5", md5, sizeof(md5)) == ESP_OK) {
        if (!Update.setMD5(md5)) {
            return fail("Invalid X-MD5 header");
        }
    } else {
        log_w("No X-MD5 header; image integrity will not be verified");
    }

    HmacSha256 hmac(key_);
    auto buffer = std::make_unique<uint8_t[]>(RECV_CHUNK_SIZE);
    size_t received = 0;
    int timeouts = 0;
    int lastPercentage = -1;
    while (received < total) {
        int len = httpd_req_recv(req, reinterpret_cast<char *>(buffer.get()),
                                 std::min(RECV_CHUNK_SIZE, total - received));
        if (len == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts <= MAX_RECV_TIMEOUTS) {
            continue;
        }
        if (len <= 0) {
            log_e("httpd_req_recv failed: %d", len);
            Update.abort();
            notify(WirelessFlashEvent::State::Failed, 0, "Connection lost");
            return ESP_FAIL;
        }
        timeouts = 0;

        hmac.update(buffer.get(), len);
        if (Update.write(buffer.get(), len) != static_cast<size_t>(len)) {
            return fail(StringFormat("Update.write: %s", Update.errorString()));
        }
        received += len;

        int percentage = static_cast<int>((received * 100) / total);
        if (percentage != lastPercentage) {
            lastPercentage = percentage;
            notify(WirelessFlashEvent::State::Receiving, percentage);
        }
    }

    // Update.end() is what switches the boot partition, so the signature must be checked first.
    if (!wirelessFlashSignatureMatches(hmac.finishHex(), signature)) {
        log_e("Wireless flash signature mismatch");
        Update.abort();
        notify(WirelessFlashEvent::State::Failed, 0, "Bad signature");
        return sendStatus(req, "403 Forbidden", "Signature mismatch (wrong .ota-key?)\n");
    }

    if (!Update.end()) {
        return fail(StringFormat("Update.end: %s", Update.errorString()));
    }

    log_i("Wireless flash complete");
    sendStatus(req, "200 OK", "Installed. Rebooting...\n");
    notify(WirelessFlashEvent::State::Installed, 100);
    return ESP_OK;
}

#endif
