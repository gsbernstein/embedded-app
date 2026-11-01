#include "WirelessFlashServer.hpp"

#if CONFIG_DEV_WIRELESS_FLASH

#include <StringUtil/StringUtil.hpp>
#include <Update.h>
#include <algorithm>
#include <esp_http_server.h>
#include <esp_ota_ops.h>
#include <memory>

#include "Version.hpp"
#include "sindarin-debug.h"

namespace {
constexpr size_t RECV_CHUNK_SIZE = 4096;
constexpr int MAX_RECV_TIMEOUTS = 3;
constexpr size_t MD5_HEX_LENGTH = 32;

auto sendStatus(httpd_req_t *req, const char *status, const std::string &body) -> esp_err_t {
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, body.c_str());
}
} // namespace

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

    if (!Update.end()) {
        return fail(StringFormat("Update.end: %s", Update.errorString()));
    }

    log_i("Wireless flash complete");
    sendStatus(req, "200 OK", "Installed. Rebooting...\n");
    notify(WirelessFlashEvent::State::Installed, 100);
    return ESP_OK;
}

#endif
