//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \file       WiFiESP.cpp

    \brief      WiFi ESP32 Wireless driver Class Implementation

    Copyright (c) 2023 Sindarin, Inc. - All Right Reserved

    The code contained in this file is confidential and may only be used
    under license and/or written consent of Sindarin.
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "WiFiESP.hpp" // Class Header

#include "WiFiESPTask.hpp" // Wifi Task
#include "config.h"
#include "sdkconfig.h"
#include "sindarin-debug.h"
// Std Libs
#include <functional>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
// Base Wifi
#include "IPAddress.h"
#include "IPv6Address.h"
#include "dhcpserver/dhcpserver_options.h"
#include "esp_smartconfig.h"
#include "esp_wifi.h"
#include "wifi_provisioning/manager.h"
// ESP Libs
#include "esp32-hal.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wpa2.h"
#include "nvs_flash.h"
// RTOS Lib
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

//-----------------------------------------------------------------------------
// Object
//-----------------------------------------------------------------------------
// Create Object
CWiFiESP WiFiESP;

//------------------------------------------------------------------------------
// Statics
//------------------------------------------------------------------------------
// STA Status
static wifiesp_status_t _sta_status = WIFIESP_STATUS_NO_WIFI;
// STA Status Group
static EventGroupHandle_t _sta_status_group = NULL;

// Is Sleep Enabled
static wifi_ps_type_t _sleepEnabled = WIFI_PS_MIN_MODEM;

// Scan Asyn
static bool _scanAsync = false;
// Scan times
static uint32_t _scanStarted = 0;
static uint32_t _scanTimeout = 10000;
static uint16_t _scanCount = 0;
// Scan result
static void *_scanResult = 0;

// default hostname
static char _default_hostname[32] = {
    0,
};

// Create a callback type
typedef struct wifiesp_event_cb_tList {
    static wifi_event_id_t current_id;
    wifi_event_id_t id;
    wifiesp_event_cb_t cb;
    wifiesp_event_func_cb_t fcb;
    wifiesp_event_cb_sys_t scb;
    wifiesp_event_id_t event;
    wifiesp_event_cb_tList()
        : id(current_id++), cb(NULL), fcb(NULL), scb(NULL), event(WIFIESP_EVENT_WIFI_READY) {}
} wifiesp_event_cb_tList_t;

// Callback ID - init to 1
wifi_event_id_t wifiesp_event_cb_tList::current_id = 1;

// Create a static std::vectors of for the callback event list
static std::vector<wifiesp_event_cb_tList_t> _eventCBList;

// Net interfaces
static esp_netif_t *_netIfs[ESP_IF_MAX] = {NULL, NULL, NULL};

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     CWiFiESP

    \brief  Constructor
    \param  N/A
    \return N/A
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
CWiFiESP::CWiFiESP() {
    // Base init variables
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     mode

    \brief  Set Wifi Mode
    \param  m WiFiMode_t
    \return bool - true=success
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool CWiFiESP::mode(wifi_mode_t m) {
    wifi_mode_t cm = getMode();
    if (cm == m) {
        return true;
    }
    if (!cm && m) {
        if (!wifiLowLevelInit(_persistent)) {
            return false;
        }
    } else if (cm && !m) {
        return espWiFiStop();
    }

    esp_err_t err;
    if (m & WIFI_MODE_STA) {
        err = set_esp_interface_hostname(ESP_IF_WIFI_STA, get_esp_netif_hostname());
        if (err) {
            log_e("Could not set hostname! %d", err);
            return false;
        }
    }
    err = esp_wifi_set_mode(m);
    if (err) {
        log_e("Could not set mode! %d", err);
        return false;
    }
    if (_long_range) {
        if (m & WIFI_MODE_STA) {
            err = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
            if (err != ESP_OK) {
                log_e("Could not enable long range on STA! %d", err);
                return false;
            }
        }
        if (m & WIFI_MODE_AP) {
            err = esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_LR);
            if (err != ESP_OK) {
                log_e("Could not enable long range on AP! %d", err);
                return false;
            }
        }
    }
    if (!espWiFiStart()) {
        return false;
    }

    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     getMode

    \brief  Get Wifi mode
    \param  N/A
    \return wifi_mode_t - current mode settings
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
wifi_mode_t CWiFiESP::getMode() {
    if (!lowLevelInitDone || !_esp_wifi_started) {
        return WIFI_MODE_NULL;
    }
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        log_w("WiFi not started");
        return WIFI_MODE_NULL;
    }
    return mode;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     enableSTA

    \brief  control STA mode
    \param  enable bool
    \return bool true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool CWiFiESP::enableSTA(bool enable) {
    wifi_mode_t currentMode = getMode();
    bool isEnabled = ((currentMode & WIFI_MODE_STA) != 0);

    if (isEnabled != enable) {
        if (enable) {
            return mode((wifi_mode_t)(currentMode | WIFI_MODE_STA));
        }
        return mode((wifi_mode_t)(currentMode & (~WIFI_MODE_STA)));
    }
    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     scanNetworks

    \brief  Start scan WiFi networks available
    \param  async run in async mode
        \param  show_hidden show hidden networks
    \return int16_t - Number of discovered networks
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
int16_t CWiFiESP::scanNetworks(bool async, bool show_hidden, bool passive, uint32_t max_ms_per_chan,
                               uint8_t channel) {
    if (getEventStatusBits() & WIFIESP_EVENTSTATUSBIT_WIFI_SCANNING) {
        return WIFI_SCAN_RUNNING;
    }

    _scanTimeout = max_ms_per_chan * 20;
    _scanAsync = async;

    enableSTA(true);

    scanDelete();

    wifi_scan_config_t config;
    config.ssid = 0;
    config.bssid = 0;
    config.channel = channel;
    config.show_hidden = show_hidden;
    if (passive) {
        config.scan_type = WIFI_SCAN_TYPE_PASSIVE;
        config.scan_time.passive = max_ms_per_chan;
    } else {
        config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
        config.scan_time.active.min = 100;
        config.scan_time.active.max = max_ms_per_chan;
    }
    if (esp_wifi_scan_start(&config, false) == ESP_OK) {
        _scanStarted = millis();
        if (!_scanStarted) { // Prevent 0 from millis overflow
            ++_scanStarted;
        }

        clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_WIFI_SCAN_DONE);
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_WIFI_SCANNING);

        if (_scanAsync) {
            return WIFI_SCAN_RUNNING;
        }
        if (waitEventStatusBits(WIFIESP_EVENTSTATUSBIT_WIFI_SCAN_DONE, 10000)) {
            return (int16_t)_scanCount;
        }
    }
    return WIFI_SCAN_FAILED;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     _scanDoneCB

    \brief  Scan completed callback
    \param  N/A
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CWiFiESP::_scanDoneCB() {
    esp_wifi_scan_get_ap_num(&(_scanCount));
    if (_scanCount) {
        _scanResult = new wifi_ap_record_t[_scanCount];
        if (!_scanResult || esp_wifi_scan_get_ap_records(
                                &(_scanCount), (wifi_ap_record_t *)_scanResult) != ESP_OK) {
            _scanCount = 0;
        }
    }
    _scanStarted = 0; // Reset after a scan is completed for normal behavior
    setEventStatusBits(WIFIESP_EVENTSTATUSBIT_WIFI_SCAN_DONE);
    clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_WIFI_SCANNING);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     scanComplete

    \brief  called to get the scan state in Async mode
    \param  N/A
    \return int16_t - -1 if scan not fin, -2 if scan not triggered
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
int16_t CWiFiESP::scanComplete() {
    if (_scanStarted &&
        (millis() - _scanStarted) > _scanTimeout) { // Check is scan was started and if the delay
                                                    // expired, return WIFI_SCAN_FAILED in this case
        clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_WIFI_SCANNING);
        return WIFI_SCAN_FAILED;
    }

    if (getEventStatusBits() & WIFIESP_EVENTSTATUSBIT_WIFI_SCAN_DONE) {
        return _scanCount;
    }

    if (getEventStatusBits() & WIFIESP_EVENTSTATUSBIT_WIFI_SCANNING) {
        return WIFI_SCAN_RUNNING;
    }

    return WIFI_SCAN_FAILED;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     scanDelete

    \brief  delete last scan result from RAM
    \param  N/A
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CWiFiESP::scanDelete() {
    clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_WIFI_SCAN_DONE);
    if (_scanResult) {
        delete[] reinterpret_cast<wifi_ap_record_t *>(_scanResult);
        _scanResult = 0;
        _scanCount = 0;
    }
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     getScanInfoByIndex

    \brief  Get scan info
    \param  i specify from which network item want to get the information
    \return bss_info * - info
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void *CWiFiESP::getScanInfoByIndex(int i) {
    if (!_scanResult || (size_t)i >= _scanCount) {
        return 0;
    }
    return reinterpret_cast<wifi_ap_record_t *>(_scanResult) + i;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     getNetworkInfo

    \brief  loads all infos from a scanned wifi in to the ptr parameters
        \param  networkItem uint8_t
        \param  ssid  const char**
        \param  encryptionType uint8_t *
        \param  RSSI int32_t *
        \param  BSSID uint8_t **
        \param  channel int32_t *
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool CWiFiESP::getNetworkInfo(uint8_t i, String &ssid, uint8_t &encType, int32_t &rssi,
                              uint8_t *&bssid, int32_t &channel) {
    wifi_ap_record_t *it = reinterpret_cast<wifi_ap_record_t *>(getScanInfoByIndex(i));
    if (!it) {
        return false;
    }
    ssid = (const char *)it->ssid;
    encType = it->authmode;
    rssi = it->rssi;
    bssid = it->bssid;
    channel = it->primary;
    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     SSID

    \brief  Return the SSID discovered during the network scan.
    \param  i specify from which network item want to get the information
    \return String - ssid string of the specified item on the networks scanned list
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
String CWiFiESP::SSID(uint8_t i) {
    wifi_ap_record_t *it = reinterpret_cast<wifi_ap_record_t *>(getScanInfoByIndex(i));
    if (!it) {
        return String();
    }
    return String(reinterpret_cast<const char *>(it->ssid));
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     encryptionType

    \brief  Return the encryption type of the networks discovered during the scanNetworks
    \param  i specify from which network item want to get the information
    \return encryption type (enum wl_enc_type) of the specified item on the networks scanned list
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
wifi_auth_mode_t CWiFiESP::encryptionType(uint8_t i) {
    wifi_ap_record_t *it = reinterpret_cast<wifi_ap_record_t *>(getScanInfoByIndex(i));
    if (!it) {
        return WIFI_AUTH_OPEN;
    }
    return it->authmode;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     RSSI

    \brief  Return the RSSI of the networks discovered during the scanNetworks
    \param  i specify from which network item want to get the information
    \return int32_t - value of RSSI of the specified item on the networks scanned list
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
int32_t CWiFiESP::RSSI(uint8_t i) {
    wifi_ap_record_t *it = reinterpret_cast<wifi_ap_record_t *>(getScanInfoByIndex(i));
    if (!it) {
        return 0;
    }
    return it->rssi;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     BSSID

    \brief  return MAC / BSSID of scanned wifi
    \param  i specify from which network item want to get the information
    \return uint8_t * MAC / BSSID of scanned wifi
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
uint8_t *CWiFiESP::BSSID(uint8_t i) {
    wifi_ap_record_t *it = reinterpret_cast<wifi_ap_record_t *>(getScanInfoByIndex(i));
    if (!it) {
        return 0;
    }
    return it->bssid;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     BSSIDstr

    \brief  return MAC / BSSID of scanned wifi
    \param  i specify from which network item want to get the information
    \return String MAC / BSSID of scanned wifi
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
String CWiFiESP::BSSIDstr(uint8_t i) {
    char mac[18] = {0};
    wifi_ap_record_t *it = reinterpret_cast<wifi_ap_record_t *>(getScanInfoByIndex(i));
    if (!it) {
        return String();
    }
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", it->bssid[0], it->bssid[1], it->bssid[2],
            it->bssid[3], it->bssid[4], it->bssid[5]);
    return String(mac);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     scan_channel

    \brief  get the channel
    \param  i specify from which network item want to get the information
    \return int32_t - channel
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
int32_t CWiFiESP::scan_channel(uint8_t i) {
    wifi_ap_record_t *it = reinterpret_cast<wifi_ap_record_t *>(getScanInfoByIndex(i));
    if (!it) {
        return 0;
    }
    return it->primary;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     setSleep

    \brief  control modem sleep when only in STA mode
    \param  enabled - enable sleep
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool CWiFiESP::setSleep(bool enabled) {
    return setSleep(enabled ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     setSleep

    \brief  set Sleep
    \param  mode wifi_ps_type_t
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool CWiFiESP::setSleep(wifi_ps_type_t sleepType) {
    if (sleepType != _sleepEnabled) {
        _sleepEnabled = sleepType;
        if ((getMode() & WIFI_MODE_STA) != 0) {
            if (esp_wifi_set_ps(_sleepEnabled) != ESP_OK) {
                log_e("esp_wifi_set_ps failed!");
                return false;
            }
        }
        return true;
    }
    return false;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     getSleep

    \brief  get modem sleep enabled
    \param  N/A
    \return bool - true if modem sleep is enabled
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
wifi_ps_type_t CWiFiESP::getSleep() { return _sleepEnabled; }

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     setTxPower

    \brief  control wifi tx power
    \param  power enum maximum wifi tx power
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool CWiFiESP::setTxPower(wifi_power_t power) {
    if ((getEventStatusBits() &
         (WIFIESP_EVENTSTATUSBIT_STA_STARTED | WIFIESP_EVENTSTATUSBIT_AP_STARTED)) == 0) {
        log_w("Neither AP or STA has been started");
        return false;
    }
    return esp_wifi_set_max_tx_power(power) == ESP_OK;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     getTxPower

    \brief  Get Tx Powere
    \param  N/A
    \return power enum maximum wifi tx power
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
wifi_power_t CWiFiESP::getTxPower() {
    int8_t power;
    if ((getEventStatusBits() &
         (WIFIESP_EVENTSTATUSBIT_STA_STARTED | WIFIESP_EVENTSTATUSBIT_AP_STARTED)) == 0) {
        log_w("Neither AP or STA has been started");
        return WIFI_POWER_19_5dBm;
    }
    if (esp_wifi_get_max_tx_power(&power)) {
        return WIFI_POWER_19_5dBm;
    }
    return (wifi_power_t)power;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     getHostname

    \brief  Get Hostname
    \param  N/A
    \return char* - hostname
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
const char *CWiFiESP::getHostname() { return get_esp_netif_hostname(); }

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     SetHostname

    \brief  Set Hostname
    \param  char* - hostname
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------

bool CWiFiESP::setHostname(const char *hostname) {
    set_esp_netif_hostname(hostname);
    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     begin

    \brief  Start Wifi connection with a WPA2 Enterprise AP
            if passphrase is set the most secure supported mode will be automatically selected
    \param ssid const char*          Pointer to the SSID string.
    \param method wpa2_method_t      The authentication method of WPA2 (WIFIESP_WPA2_AUTH_TLS,
   WIFIESP_WPA2_AUTH_PEAP, WIFIESP_WPA2_AUTH_TTLS) \param wpa2_identity  const char* Pointer to the
   entity \param wpa2_username  const char*          Pointer to the username \param password const
   char *     Pointer to the password. \param ca_pem const char*        Pointer to a string with the
   contents of a  .pem  file with CA cert \param client_crt const char*        Pointer to a string
   with the contents of a .crt file with client cert \param client_key const char*        Pointer to
   a string with the contants of a .key file with client key \param bssid uint8_t[6] Optional. BSSID
   / MAC of AP \param channel                   Optional. Channel of AP \param connect Optional.
   call connect \return wifiesp_status_t - status
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
wifiesp_status_t CWiFiESP::begin(const char *wpa2_ssid, wifiesp_wpa2_auth_method_t method,
                                 const char *wpa2_identity, const char *wpa2_username,
                                 const char *wpa2_password, const char *ca_pem,
                                 const char *client_crt, const char *client_key, int32_t channel,
                                 const uint8_t *bssid, bool connect) {
    if (!enableSTA(true)) {
        log_e("STA enable failed!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    if (!wpa2_ssid || *wpa2_ssid == 0x00 || strlen(wpa2_ssid) > 32) {
        log_e("SSID too long or missing!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    if (wpa2_identity && strlen(wpa2_identity) > 64) {
        log_e("identity too long!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    if (wpa2_username && strlen(wpa2_username) > 64) {
        log_e("username too long!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    if (wpa2_password && strlen(wpa2_password) > 64) {
        log_e("password too long!");
    }

    if (ca_pem) {
        esp_wifi_sta_wpa2_ent_set_ca_cert((uint8_t *)ca_pem, strlen(ca_pem));
    }

    if (client_crt) {
        esp_wifi_sta_wpa2_ent_set_cert_key((uint8_t *)client_crt, strlen(client_crt),
                                           (uint8_t *)client_key, strlen(client_key), NULL, 0);
    }

    esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)wpa2_identity, strlen(wpa2_identity));
    if (method == WIFIESP_WPA2_AUTH_PEAP || method == WIFIESP_WPA2_AUTH_TTLS) {
        esp_wifi_sta_wpa2_ent_set_username((uint8_t *)wpa2_username, strlen(wpa2_username));
        esp_wifi_sta_wpa2_ent_set_password((uint8_t *)wpa2_password, strlen(wpa2_password));
    }
    esp_wifi_sta_wpa2_ent_enable(); // set config settings to enable function
    begin(wpa2_ssid);               // connect to wifi

    return status();
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     begin

    \brief  Start Wifi connection with a WPA2 Enterprise AP
            if passphrase is set the most secure supported mode will be automatically selected
    \param ssid const char*          Pointer to the SSID string.
    \param passphrase const char *   Optional. Passphrase. Valid characters in a passphrase must be
   between ASCII 32-126 (decimal). \param bssid uint8_t[6]          Optional. BSSID / MAC of AP
    \param channel                   Optional. Channel of AP
    \param connect                   Optional. call connect
    \return wifiesp_status_t - status
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
wifiesp_status_t CWiFiESP::begin(const char *ssid, const char *passphrase, int32_t channel,
                                 const uint8_t *bssid, bool connect) {

    if (!enableSTA(true)) {
        log_e("STA enable failed!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    if (!ssid || *ssid == 0x00 || strlen(ssid) > 32) {
        log_e("SSID too long or missing!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    if (passphrase && strlen(passphrase) > 64) {
        log_e("passphrase too long!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    wifi_config_t conf;
    memset(&conf, 0, sizeof(wifi_config_t));
    _wifi_strncpy(reinterpret_cast<char *>(conf.sta.ssid), ssid, 32);

    if (passphrase) {
        _wifi_strncpy(reinterpret_cast<char *>(conf.sta.password), passphrase, 64);
    }

    if (channel == 0) {
        // If no specific channel specified, then do an slower WIFI_ALL_CHANNEL_SCAN
        wifi_sta_config(&conf, ssid, passphrase, bssid, channel, WIFI_ALL_CHANNEL_SCAN);
    } else
        wifi_sta_config(&conf, ssid, passphrase, bssid, channel, WIFI_FAST_SCAN);

    wifi_config_t current_conf;
    if (esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &current_conf) != ESP_OK) {
        log_e("get current config failed!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }
    if (!sta_config_equal(current_conf, conf)) {
        if (esp_wifi_disconnect()) {
            log_e("disconnect failed!");
            return WIFIESP_STATUS_CONNECT_FAILED;
        }

        if (esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &conf) != ESP_OK) {
            log_e("set config failed!");
            return WIFIESP_STATUS_CONNECT_FAILED;
        }
    } else if (status() == WIFIESP_STATUS_CONNECTED) {
        return WIFIESP_STATUS_CONNECTED;
    } else {
        if (esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &conf) != ESP_OK) {
            log_e("set config failed!");
            return WIFIESP_STATUS_CONNECT_FAILED;
        }
    }

    // if(!_useStaticIp){
    if (set_esp_interface_ip(ESP_IF_WIFI_STA) != ESP_OK) {
        return WIFIESP_STATUS_CONNECT_FAILED;
    }
    //}

    if (connect) {
        if (esp_wifi_connect() != ESP_OK) {
            log_e("connect failed!");
            return WIFIESP_STATUS_CONNECT_FAILED;
        }
    }

    return status();
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     begin

    \brief  Start Wifi connection with a WPA2 Enterprise AP
            if passphrase is set the most secure supported mode will be automatically selected
    \param ssid char*                Pointer to the SSID string.
    \param passphrase char *         Optional. Passphrase. Valid characters in a passphrase must be
   between ASCII 32-126 (decimal). \param bssid uint8_t[6]          Optional. BSSID / MAC of AP
    \param channel                   Optional. Channel of AP
    \param connect                   Optional. call connect
    \return wifiesp_status_t - status
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
wifiesp_status_t CWiFiESP::begin(char *ssid, char *passphrase, int32_t channel,
                                 const uint8_t *bssid, bool connect) {
    return begin((const char *)ssid, (const char *)passphrase, channel, bssid, connect);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     begin

    \brief  Use to connect to SDK config.
    \param N/A
    \return wifiesp_status_t - status
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
wifiesp_status_t CWiFiESP::begin() {

    if (!enableSTA(true)) {
        log_e("STA enable failed!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    wifi_config_t current_conf;
    if (esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &current_conf) != ESP_OK ||
        esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &current_conf) != ESP_OK) {
        log_e("config failed");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    // if(!_useStaticIp && set_esp_interface_ip(ESP_IF_WIFI_STA) != ESP_OK) {
    if (set_esp_interface_ip(ESP_IF_WIFI_STA) != ESP_OK) {
        log_e("set ip failed!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    if (status() != WIFIESP_STATUS_CONNECTED) {
        esp_err_t err = esp_wifi_connect();
        if (err) {
            log_e("connect failed! 0x%x", err);
            return WIFIESP_STATUS_CONNECT_FAILED;
        }
    }

    return status();
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
        \fn     setAutoReconnect

        \brief  Set Autoreconnect
        \param  autoReconnect - setting
        \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool CWiFiESP::setAutoReconnect(bool autoReconnect) {
    _autoReconnect = autoReconnect;
    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
        \fn     getAutoReconnect

        \brief  Get Autoreconnect
        \param  N/A
        \return bool - autoReconnect setting
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool CWiFiESP::getAutoReconnect() { return _autoReconnect; }

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     reconnect

    \brief  will force a disconnect and then start reconnecting to AP
    \param  N/A
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool CWiFiESP::reconnect() {
    if (getMode() & WIFI_MODE_STA) {
        if (esp_wifi_disconnect() == ESP_OK) {
            return esp_wifi_connect() == ESP_OK;
        }
    }
    return false;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     disconnect

    \brief  Disconnect from the network
    \param  wifioff - turn off
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool CWiFiESP::disconnect(bool wifioff, bool eraseap) {
    wifi_config_t conf;
    wifi_sta_config(&conf);

    if (getMode() & WIFI_MODE_STA) {
        if (eraseap) {
            if (esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &conf)) {
                log_e("clear config failed!");
            }
        }
        if (esp_wifi_disconnect()) {
            log_e("disconnect failed!");
            return false;
        }
        if (wifioff) {
            return enableSTA(false);
        }
        return true;
    }

    return false;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     isConnected

    \brief  is STA interface connected
    \param  N/A
    \return bool - true if STA is connected to an AP
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool CWiFiESP::isConnected() { return (status() == WIFIESP_STATUS_CONNECTED); }

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     localIP

    \brief  Get the station interface IP address.
    \param  N/A
    \return IPAddress station IP
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
IPAddress CWiFiESP::localIP() {
    if (getMode() == WIFI_MODE_NULL) {
        return IPAddress();
    }
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(get_esp_interface_netif(ESP_IF_WIFI_STA), &ip) != ESP_OK) {
        log_e("Netif Get IP Failed!");
        return IPAddress();
    }
    return IPAddress(ip.ip.addr);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     _setStatus

    \brief  Set STA Status
    \param  status - status to set
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CWiFiESP::_setStatus(wifiesp_status_t status) {
    if (!_sta_status_group) {
        _sta_status_group = xEventGroupCreate();
        if (!_sta_status_group) {
            log_e("STA Status Group Create Failed!");
            _sta_status = status;
            return;
        }
    }
    xEventGroupClearBits(_sta_status_group, 0x00FFFFFF);
    xEventGroupSetBits(_sta_status_group, status);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     status

    \brief  Return STA Connection status
    \param  N/A
    \return one of the value defined in wifiesp_status_t
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
wifiesp_status_t CWiFiESP::status() {
    if (!_sta_status_group) {
        return _sta_status;
    }
    return (wifiesp_status_t)xEventGroupClearBits(_sta_status_group, 0);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     _eventCallback

    \brief  callback for WiFi events
    \param  event - event for the vcallback
    \return esp_err_t - error
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
// Debug output logging
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
const char *wifiesp_event_names[] = {"WIFI_READY",
                                     "SCAN_DONE",
                                     "STA_START",
                                     "STA_STOP",
                                     "STA_CONNECTED",
                                     "STA_DISCONNECTED",
                                     "STA_AUTHMODE_CHANGE",
                                     "STA_GOT_IP",
                                     "STA_GOT_IP6",
                                     "STA_LOST_IP",
                                     "AP_START",
                                     "AP_STOP",
                                     "AP_STACONNECTED",
                                     "AP_STADISCONNECTED",
                                     "AP_STAIPASSIGNED",
                                     "AP_PROBEREQRECVED",
                                     "AP_GOT_IP6",
                                     "FTM_REPORT",
                                     "ETH_START",
                                     "ETH_STOP",
                                     "ETH_CONNECTED",
                                     "ETH_DISCONNECTED",
                                     "ETH_GOT_IP",
                                     "ETH_GOT_IP6",
                                     "WPS_ER_SUCCESS",
                                     "WPS_ER_FAILED",
                                     "WPS_ER_TIMEOUT",
                                     "WPS_ER_PIN",
                                     "WPS_ER_PBC_OVERLAP",
                                     "SC_SCAN_DONE",
                                     "SC_FOUND_CHANNEL",
                                     "SC_GOT_SSID_PSWD",
                                     "SC_SEND_ACK_DONE",
                                     "PROV_INIT",
                                     "PROV_DEINIT",
                                     "PROV_START",
                                     "PROV_END",
                                     "PROV_CRED_RECV",
                                     "PROV_CRED_FAIL",
                                     "PROV_CRED_SUCCESS"};
#endif
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_WARN
const char *system_event_reasons[] = {"UNSPECIFIED",
                                      "AUTH_EXPIRE",
                                      "AUTH_LEAVE",
                                      "ASSOC_EXPIRE",
                                      "ASSOC_TOOMANY",
                                      "NOT_AUTHED",
                                      "NOT_ASSOCED",
                                      "ASSOC_LEAVE",
                                      "ASSOC_NOT_AUTHED",
                                      "DISASSOC_PWRCAP_BAD",
                                      "DISASSOC_SUPCHAN_BAD",
                                      "UNSPECIFIED",
                                      "IE_INVALID",
                                      "MIC_FAILURE",
                                      "4WAY_HANDSHAKE_TIMEOUT",
                                      "GROUP_KEY_UPDATE_TIMEOUT",
                                      "IE_IN_4WAY_DIFFERS",
                                      "GROUP_CIPHER_INVALID",
                                      "PAIRWISE_CIPHER_INVALID",
                                      "AKMP_INVALID",
                                      "UNSUPP_RSN_IE_VERSION",
                                      "INVALID_RSN_IE_CAP",
                                      "802_1X_AUTH_FAILED",
                                      "CIPHER_SUITE_REJECTED",
                                      "BEACON_TIMEOUT",
                                      "NO_AP_FOUND",
                                      "AUTH_FAIL",
                                      "ASSOC_FAIL",
                                      "HANDSHAKE_TIMEOUT",
                                      "CONNECTION_FAIL"};
#define reason2str(r) ((r > 176) ? system_event_reasons[r - 176] : system_event_reasons[r - 1])
#endif
// callback
esp_err_t CWiFiESP::_eventCallback(wifiesp_event_t *event) {
    static bool first_connect = true;

    if (event->event_id < WIFIESP_EVENT_MAX) {
        log_d("WiFiESP Event: %d - %s", event->event_id, wifiesp_event_names[event->event_id]);
    }
    if (event->event_id == WIFIESP_EVENT_WIFI_SCAN_DONE) {
        _scanDoneCB();

    } else if (event->event_id == WIFIESP_EVENT_WIFI_STA_START) {
        _setStatus(WIFIESP_STATUS_DISCONNECTED);
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_STA_STARTED);
        if (esp_wifi_set_ps(_sleepEnabled) != ESP_OK) {
            log_e("esp_wifi_set_ps failed");
        }
    } else if (event->event_id == WIFIESP_EVENT_WIFI_STA_STOP) {
        _setStatus(WIFIESP_STATUS_NO_WIFI);
        clearEventStatusBits(
            WIFIESP_EVENTSTATUSBIT_STA_STARTED | WIFIESP_EVENTSTATUSBIT_STA_CONNECTED |
            WIFIESP_EVENTSTATUSBIT_STA_HAS_IP | WIFIESP_EVENTSTATUSBIT_STA_HAS_IP6);
    } else if (event->event_id == WIFIESP_EVENT_WIFI_STA_CONNECTED) {
        _setStatus(WIFIESP_STATUS_IDLE_STATUS);
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_STA_CONNECTED);

        // esp_netif_create_ip6_linklocal(_netIfs[ESP_IF_WIFI_STA]);
    } else if (event->event_id == WIFIESP_EVENT_WIFI_STA_DISCONNECTED) {
        uint8_t reason = event->event_info.wifi_sta_disconnected.reason;
        log_w("Reason: %u - %s", reason, reason2str(reason));
        if (reason == WIFI_REASON_NO_AP_FOUND) {
            _setStatus(WIFIESP_STATUS_NO_SSID_AVAIL);
        } else if ((reason == WIFI_REASON_AUTH_FAIL) && !first_connect) {
            _setStatus(WIFIESP_STATUS_CONNECT_FAILED);
        } else if (reason == WIFI_REASON_BEACON_TIMEOUT ||
                   reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {
            _setStatus(WIFIESP_STATUS_CONNECTION_LOST);
        } else if (reason == WIFI_REASON_AUTH_EXPIRE) {

        } else {
            _setStatus(WIFIESP_STATUS_DISCONNECTED);
        }
        clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_STA_CONNECTED |
                             WIFIESP_EVENTSTATUSBIT_STA_HAS_IP |
                             WIFIESP_EVENTSTATUSBIT_STA_HAS_IP6);
        if (first_connect &&
            ((reason == WIFI_REASON_AUTH_EXPIRE) || (reason >= WIFI_REASON_BEACON_TIMEOUT))) {
            log_d("WiFi Reconnect Running");
            WiFiESP.disconnect();
            WiFiESP.begin();
            first_connect = false;
        } else if (WiFiESP.getAutoReconnect()) {
            if ((reason == WIFI_REASON_AUTH_EXPIRE) ||
                (reason >= WIFI_REASON_BEACON_TIMEOUT && reason != WIFI_REASON_AUTH_FAIL)) {
                log_d("WiFi AutoReconnect Running");
                WiFiESP.disconnect();
                WiFiESP.begin();
            }
        } else if (reason == WIFI_REASON_ASSOC_FAIL) {
            _setStatus(WIFIESP_STATUS_CONNECT_FAILED);
        }
    } else if (event->event_id == WIFIESP_EVENT_WIFI_STA_GOT_IP) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
        uint8_t *ip = (uint8_t *)&(event->event_info.got_ip.ip_info.ip.addr);
        uint8_t *mask = (uint8_t *)&(event->event_info.got_ip.ip_info.netmask.addr);
        uint8_t *gw = (uint8_t *)&(event->event_info.got_ip.ip_info.gw.addr);
        log_d("STA IP: %u.%u.%u.%u, MASK: %u.%u.%u.%u, GW: %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3],
              mask[0], mask[1], mask[2], mask[3], gw[0], gw[1], gw[2], gw[3]);
#endif
        _setStatus(WIFIESP_STATUS_CONNECTED);
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_STA_HAS_IP |
                           WIFIESP_EVENTSTATUSBIT_STA_CONNECTED);
    } else if (event->event_id == WIFIESP_EVENT_WIFI_STA_LOST_IP) {
        _setStatus(WIFIESP_STATUS_IDLE_STATUS);
        clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_STA_HAS_IP);

    } else if (event->event_id == WIFIESP_EVENT_WIFI_AP_START) {
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_AP_STARTED);
    } else if (event->event_id == WIFIESP_EVENT_WIFI_AP_STOP) {
        clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_AP_STARTED |
                             WIFIESP_EVENTSTATUSBIT_AP_HAS_CLIENT);
    } else if (event->event_id == WIFIESP_EVENT_WIFI_AP_STACONNECTED) {
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_AP_HAS_CLIENT);
    } else if (event->event_id == WIFIESP_EVENT_WIFI_AP_STADISCONNECTED) {
        wifi_sta_list_t clients;
        if (esp_wifi_ap_get_sta_list(&clients) != ESP_OK || !clients.num) {
            clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_AP_HAS_CLIENT);
        }

    } else if (event->event_id == WIFIESP_EVENT_ETH_START) {
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_ETH_STARTED);
    } else if (event->event_id == WIFIESP_EVENT_ETH_STOP) {
        clearEventStatusBits(
            WIFIESP_EVENTSTATUSBIT_ETH_STARTED | WIFIESP_EVENTSTATUSBIT_ETH_CONNECTED |
            WIFIESP_EVENTSTATUSBIT_ETH_HAS_IP | WIFIESP_EVENTSTATUSBIT_ETH_HAS_IP6);
    } else if (event->event_id == WIFIESP_EVENT_ETH_CONNECTED) {
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_ETH_CONNECTED);
    } else if (event->event_id == WIFIESP_EVENT_ETH_DISCONNECTED) {
        clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_ETH_CONNECTED |
                             WIFIESP_EVENTSTATUSBIT_ETH_HAS_IP |
                             WIFIESP_EVENTSTATUSBIT_ETH_HAS_IP6);
    } else if (event->event_id == WIFIESP_EVENT_ETH_GOT_IP) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
        uint8_t *ip = (uint8_t *)&(event->event_info.got_ip.ip_info.ip.addr);
        uint8_t *mask = (uint8_t *)&(event->event_info.got_ip.ip_info.netmask.addr);
        uint8_t *gw = (uint8_t *)&(event->event_info.got_ip.ip_info.gw.addr);
        log_d("ETH IP: %u.%u.%u.%u, MASK: %u.%u.%u.%u, GW: %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3],
              mask[0], mask[1], mask[2], mask[3], gw[0], gw[1], gw[2], gw[3]);
#endif
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_ETH_CONNECTED |
                           WIFIESP_EVENTSTATUSBIT_ETH_HAS_IP);

    } else if (event->event_id == WIFIESP_EVENT_WIFI_STA_GOT_IP6) {
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_STA_CONNECTED |
                           WIFIESP_EVENTSTATUSBIT_STA_HAS_IP6);
    } else if (event->event_id == WIFIESP_EVENT_WIFI_AP_GOT_IP6) {
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_AP_HAS_IP6);
    } else if (event->event_id == WIFIESP_EVENT_ETH_GOT_IP6) {
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_ETH_CONNECTED |
                           WIFIESP_EVENTSTATUSBIT_ETH_HAS_IP6);
    } else if (event->event_id == WIFIESP_EVENT_SC_GOT_SSID_PSWD) {
        WiFiESP.begin((const char *)event->event_info.sc_got_ssid_pswd.ssid,
                      (const char *)event->event_info.sc_got_ssid_pswd.password, 0,
                      ((event->event_info.sc_got_ssid_pswd.bssid_set == true)
                           ? event->event_info.sc_got_ssid_pswd.bssid
                           : NULL));
    }
    // else if(event->event_id == WIFIESP_EVENT_SC_SEND_ACK_DONE) {
    //    esp_smartconfig_stop();
    //    _smartConfigDone = true;
    //}

    for (uint32_t i = 0; i < _eventCBList.size(); i++) {
        wifiesp_event_cb_tList_t entry = _eventCBList[i];
        if (entry.cb || entry.fcb || entry.scb) {
            if (entry.event == (wifiesp_event_id_t)event->event_id ||
                entry.event == WIFIESP_EVENT_MAX) {
                if (entry.cb) {
                    entry.cb((wifiesp_event_id_t)event->event_id);
                } else if (entry.fcb) {
                    entry.fcb((wifiesp_event_id_t)event->event_id,
                              (wifiesp_event_info_t)event->event_info);
                } else {
                    entry.scb(event);
                }
            }
        }
    }
    return ESP_OK;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     onEvent

    \brief  set callback function
    \param  cbEvent - callback for event
    \param  event - event for callback
    \return wifi_event_id_t - id
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
wifi_event_id_t CWiFiESP::onEvent(wifiesp_event_cb_t cbEvent, wifiesp_event_id_t event) {
    if (!cbEvent) {
        return 0;
    }
    wifiesp_event_cb_tList_t newEventHandler;
    newEventHandler.cb = cbEvent;
    newEventHandler.fcb = NULL;
    newEventHandler.scb = NULL;
    newEventHandler.event = event;
    _eventCBList.push_back(newEventHandler);
    return newEventHandler.id;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     onEvent

    \brief  set callback function
    \param  cbEvent - callback for event
    \param  event - event for callback
    \return wifi_event_id_t - id
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
wifi_event_id_t CWiFiESP::onEvent(wifiesp_event_func_cb_t cbEvent, wifiesp_event_id_t event) {
    if (!cbEvent) {
        return 0;
    }
    wifiesp_event_cb_tList_t newEventHandler;
    newEventHandler.cb = NULL;
    newEventHandler.fcb = cbEvent;
    newEventHandler.scb = NULL;
    newEventHandler.event = event;
    _eventCBList.push_back(newEventHandler);
    return newEventHandler.id;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     onEvent

    \brief  set callback function
    \param  cbEvent - callback for event
    \param  event - event for callback
    \return wifi_event_id_t - id
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
wifi_event_id_t CWiFiESP::onEvent(wifiesp_event_cb_sys_t cbEvent, wifiesp_event_id_t event) {
    if (!cbEvent) {
        return 0;
    }
    wifiesp_event_cb_tList_t newEventHandler;
    newEventHandler.cb = NULL;
    newEventHandler.fcb = NULL;
    newEventHandler.scb = cbEvent;
    newEventHandler.event = event;
    _eventCBList.push_back(newEventHandler);
    return newEventHandler.id;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     removeEvent

    \brief  removes a callback form event handler
    \param  cbEvent - callback for event
    \param  event - event optional filter (WIFI_EVENT_MAX is all events)
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CWiFiESP::removeEvent(wifiesp_event_cb_t cbEvent, wifiesp_event_id_t event) {
    if (!cbEvent) {
        return;
    }

    for (uint32_t i = 0; i < _eventCBList.size(); i++) {
        wifiesp_event_cb_tList_t entry = _eventCBList[i];
        if (entry.cb == cbEvent && entry.event == event) {
            _eventCBList.erase(_eventCBList.begin() + i);
        }
    }
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     removeEvent

    \brief  removes a callback form event handler
    \param  cbEvent - callback for event
    \param  event - event optional filter (WIFI_EVENT_MAX is all events)
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CWiFiESP::removeEvent(wifiesp_event_cb_sys_t cbEvent, wifiesp_event_id_t event) {
    if (!cbEvent) {
        return;
    }

    for (uint32_t i = 0; i < _eventCBList.size(); i++) {
        wifiesp_event_cb_tList_t entry = _eventCBList[i];
        if (entry.scb == cbEvent && entry.event == event) {
            _eventCBList.erase(_eventCBList.begin() + i);
        }
    }
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     removeEvent

    \brief  removes a callback form event handler
    \param  cbEvent - callback for event
    \param  event - event optional filter (WIFI_EVENT_MAX is all events)
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CWiFiESP::removeEvent(wifi_event_id_t id) {
    for (uint32_t i = 0; i < _eventCBList.size(); i++) {
        wifiesp_event_cb_tList_t entry = _eventCBList[i];
        if (entry.id == id) {
            _eventCBList.erase(_eventCBList.begin() + i);
        }
    }
}

//-----------------------------------------------------------------------------
// PRIVATE
//-----------------------------------------------------------------------------

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     tcpipInit

    \brief  Init the TCPIP setups
    \param  N/A
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool CWiFiESP::tcpipInit() {
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
#if CONFIG_IDF_TARGET_ESP32
        uint8_t mac[8];
        if (esp_efuse_mac_get_default(mac) == ESP_OK) {
            esp_base_mac_addr_set(mac);
        }
#endif
        initialized = esp_netif_init() == ESP_OK;
        if (initialized) {
            initialized = WiFiESPTask_StartNetworkEventTask();
        } else {
            log_e("esp_netif_init failed!");
        }
    }
    return initialized;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     wifiLowLevelInit

    \brief  Init the low level
    \param  persistent
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
// Temporary fix to ensure that CDC+JTAG stay on on ESP32-C3
#if CONFIG_IDF_TARGET_ESP32C3
extern "C" void phy_bbpll_en_usb(bool en);
#endif
bool CWiFiESP::wifiLowLevelInit(bool persistent) {
    if (!lowLevelInitDone) {
        lowLevelInitDone = true;
        if (!tcpipInit()) {
            lowLevelInitDone = false;
            return lowLevelInitDone;
        }
        if (_netIfs[ESP_IF_WIFI_AP] == NULL) {
            _netIfs[ESP_IF_WIFI_AP] = esp_netif_create_default_wifi_ap();
        }
        if (_netIfs[ESP_IF_WIFI_STA] == NULL) {
            _netIfs[ESP_IF_WIFI_STA] = esp_netif_create_default_wifi_sta();
        }

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

        // Do not use Static Buffers
        cfg.static_tx_buf_num = 0;
        cfg.dynamic_tx_buf_num = 32;
        cfg.tx_buf_type = 1;
        cfg.cache_tx_buf_num = 1; // can't be zero!
        cfg.static_rx_buf_num = 4;
        cfg.dynamic_rx_buf_num = 32;

        esp_err_t err = esp_wifi_init(&cfg);
        if (err) {
            log_e("esp_wifi_init %d", err);
            lowLevelInitDone = false;
            return lowLevelInitDone;
        }
// Temporary fix to ensure that CDC+JTAG stay on on ESP32-C3
#if CONFIG_IDF_TARGET_ESP32C3
        phy_bbpll_en_usb(true);
#endif
        if (!persistent) {
            lowLevelInitDone = esp_wifi_set_storage(WIFI_STORAGE_RAM) == ESP_OK;
        }
        if (lowLevelInitDone) {
            wifiesp_event_t wifiesp_event;
            wifiesp_event.event_id = WIFIESP_EVENT_WIFI_READY;
            WiFiESPTask_PostEvent(&wifiesp_event);
        }
    }
    return lowLevelInitDone;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     wifiLowLevelDeinit

    \brief  De-init the low level
    \param  N/A
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool CWiFiESP::wifiLowLevelDeinit() {
    if (lowLevelInitDone) {
        lowLevelInitDone = !(esp_wifi_deinit() == ESP_OK);
    }
    return !lowLevelInitDone;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     espWiFiStart

    \brief  Start the low level ESP Wifi
    \param  N/A
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool CWiFiESP::espWiFiStart() {
    if (_esp_wifi_started) {
        return true;
    }
    _esp_wifi_started = true;
    esp_err_t err = esp_wifi_start();
    if (err != ESP_OK) {
        _esp_wifi_started = false;
        log_e("esp_wifi_start %d", err);
        return _esp_wifi_started;
    }
    return _esp_wifi_started;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     espWiFiStop

    \brief  Stop the low level ESP Wifi
    \param  N/A
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool CWiFiESP::espWiFiStop() {
    esp_err_t err;
    if (!_esp_wifi_started) {
        return true;
    }
    _esp_wifi_started = false;
    err = esp_wifi_stop();
    if (err) {
        log_e("Could not stop WiFi! %d", err);
        _esp_wifi_started = true;
        return false;
    }
    return wifiLowLevelDeinit();
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     EventStatusBits Get,Set,Wait

    \brief  Interact with the event status bits
    \param  N/A
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
int CWiFiESP::setEventStatusBits(int bits) { return WiFiESPTask_SetEventStatusBits(bits); }

int CWiFiESP::clearEventStatusBits(int bits) { return WiFiESPTask_ClearEventStatusBits(bits); }

int CWiFiESP::getEventStatusBits() { return WiFiESPTask_GetEventStatusBits(); }

int CWiFiESP::waitEventStatusBits(int bits, uint32_t timeout_ms) {
    return WiFiESPTask_WaitEventStatusBits(bits, // The bits within the event group to wait for.
                                           timeout_ms / portTICK_PERIOD_MS) &
           bits; // Wait a maximum of 100ms for either bit to be set.
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     netif_interface

    \brief  Interact with the network interface settings
    \param  N/A
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
esp_interface_t CWiFiESP::get_esp_netif_interface(esp_netif_t *esp_netif) {
    for (int i = 0; i < ESP_IF_MAX; i++) {
        if (_netIfs[i] != NULL && _netIfs[i] == esp_netif) {
            return (esp_interface_t)i;
        }
    }
    return ESP_IF_MAX;
}

void CWiFiESP::add_esp_interface_netif(esp_interface_t interface, esp_netif_t *esp_netif) {
    if (interface < ESP_IF_MAX) {
        _netIfs[interface] = esp_netif;
    }
}

esp_netif_t *CWiFiESP::get_esp_interface_netif(esp_interface_t interface) {
    if (interface < ESP_IF_MAX) {
        return _netIfs[interface];
    }
    return NULL;
}

esp_err_t CWiFiESP::set_esp_interface_hostname(esp_interface_t interface, const char *hostname) {
    if (interface < ESP_IF_MAX) {
        return esp_netif_set_hostname(_netIfs[interface], hostname);
    }
    return ESP_FAIL;
}

esp_err_t CWiFiESP::set_esp_interface_ip(esp_interface_t interface, IPAddress local_ip,
                                         IPAddress gateway, IPAddress subnet) {
    esp_netif_t *esp_netif = _netIfs[interface];
    esp_netif_dhcp_status_t status = ESP_NETIF_DHCP_INIT;
    esp_netif_ip_info_t info;
    info.ip.addr = static_cast<uint32_t>(local_ip);
    info.gw.addr = static_cast<uint32_t>(gateway);
    info.netmask.addr = static_cast<uint32_t>(subnet);

    log_v("Configuring %s static IP: " IPSTR ", MASK: " IPSTR ", GW: " IPSTR,
          interface == ESP_IF_WIFI_STA  ? "Station"
          : interface == ESP_IF_WIFI_AP ? "SoftAP"
                                        : "Ethernet",
          IP2STR(&info.ip), IP2STR(&info.netmask), IP2STR(&info.gw));

    esp_err_t err = ESP_OK;
    if (interface != ESP_IF_WIFI_AP) {
        err = esp_netif_dhcpc_get_status(esp_netif, &status);
        if (err) {
            log_e("DHCPC Get Status Failed! 0x%04x", err);
            return err;
        }
        err = esp_netif_dhcpc_stop(esp_netif);
        if (err && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
            log_e("DHCPC Stop Failed! 0x%04x", err);
            return err;
        }
        err = esp_netif_set_ip_info(esp_netif, &info);
        if (err) {
            log_e("Netif Set IP Failed! 0x%04x", err);
            return err;
        }
        if (info.ip.addr == 0) {
            err = esp_netif_dhcpc_start(esp_netif);
            if (err) {
                log_e("DHCPC Start Failed! 0x%04x", err);
                return err;
            }
        }
    } else {
        err = esp_netif_dhcps_get_status(esp_netif, &status);
        if (err) {
            log_e("DHCPS Get Status Failed! 0x%04x", err);
            return err;
        }
        err = esp_netif_dhcps_stop(esp_netif);
        if (err && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
            log_e("DHCPS Stop Failed! 0x%04x", err);
            return err;
        }
        err = esp_netif_set_ip_info(esp_netif, &info);
        if (err) {
            log_e("Netif Set IP Failed! 0x%04x", err);
            return err;
        }

        dhcps_lease_t lease;
        lease.enable = true;
        uint32_t dhcp_ipaddr = static_cast<uint32_t>(local_ip);
        // prevents DHCP lease range to overflow subnet/24 range
        // there will be 11 addresses for DHCP to lease
        uint8_t leaseStart = (uint8_t)(~subnet[3] - 12);
        if ((local_ip[3]) < leaseStart) {
            lease.start_ip.addr = dhcp_ipaddr + (1 << 24);
            lease.end_ip.addr = dhcp_ipaddr + (11 << 24);
        } else {
            // make range stay in the begining of the netmask range
            dhcp_ipaddr = (dhcp_ipaddr & 0x00FFFFFF);
            lease.start_ip.addr = dhcp_ipaddr + (1 << 24);
            lease.end_ip.addr = dhcp_ipaddr + (11 << 24);
        }
        log_v("DHCP Server Range: %s to %s", IPAddress(lease.start_ip.addr).toString().c_str(),
              IPAddress(lease.end_ip.addr).toString().c_str());
        err = tcpip_adapter_dhcps_option((tcpip_adapter_dhcp_option_mode_t)TCPIP_ADAPTER_OP_SET,
                                         (tcpip_adapter_dhcp_option_id_t)REQUESTED_IP_ADDRESS,
                                         (void *)&lease, sizeof(dhcps_lease_t));
        if (err) {
            log_e("DHCPS Set Lease Failed! 0x%04x", err);
            return err;
        }

        err = esp_netif_dhcps_start(esp_netif);
        if (err) {
            log_e("DHCPS Start Failed! 0x%04x", err);
            return err;
        }
    }
    return err;
}

esp_err_t CWiFiESP::set_esp_interface_dns(esp_interface_t interface, IPAddress main_dns,
                                          IPAddress backup_dns, IPAddress fallback_dns) {
    esp_netif_t *esp_netif = _netIfs[interface];
    esp_netif_dns_info_t dns;
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = static_cast<uint32_t>(main_dns);
    if (dns.ip.u_addr.ip4.addr &&
        esp_netif_set_dns_info(esp_netif, ESP_NETIF_DNS_MAIN, &dns) != ESP_OK) {
        log_e("Set Main DNS Failed!");
        return ESP_FAIL;
    }
    if (interface != ESP_IF_WIFI_AP) {
        dns.ip.u_addr.ip4.addr = static_cast<uint32_t>(backup_dns);
        if (dns.ip.u_addr.ip4.addr &&
            esp_netif_set_dns_info(esp_netif, ESP_NETIF_DNS_BACKUP, &dns) != ESP_OK) {
            log_e("Set Backup DNS Failed!");
            return ESP_FAIL;
        }
        dns.ip.u_addr.ip4.addr = static_cast<uint32_t>(fallback_dns);
        if (dns.ip.u_addr.ip4.addr &&
            esp_netif_set_dns_info(esp_netif, ESP_NETIF_DNS_FALLBACK, &dns) != ESP_OK) {
            log_e("Set Fallback DNS Failed!");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

const char *CWiFiESP::get_esp_netif_hostname() {
    if (_default_hostname[0] == 0) {
        uint8_t eth_mac[6];
        esp_wifi_get_mac((wifi_interface_t)WIFI_IF_STA, eth_mac);
        snprintf(_default_hostname, 32, "%s%02X%02X%02X", CONFIG_IDF_TARGET "-", eth_mac[3],
                 eth_mac[4], eth_mac[5]);
    }
    return (const char *)_default_hostname;
}

void CWiFiESP::set_esp_netif_hostname(const char *name) {
    if (name) {
        snprintf(_default_hostname, 32, "%s", name);
    }
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     _wifi_strncpy

    \brief  Copy the wifi string
    \param  dst - destination
        \param  src - source
        \dst_len - detination length
    \return length copied
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
size_t CWiFiESP::_wifi_strncpy(char *dst, const char *src, size_t dst_len) {
    if (!dst || !src || !dst_len) {
        return 0;
    }
    size_t src_len = strlen(src);
    if (src_len >= dst_len) {
        src_len = dst_len;
    } else {
        src_len += 1;
    }
    memcpy(dst, src, src_len);
    return src_len;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     sta_config_equal

    \brief  compare two STA configurations
    \param lhs station_config
    \param rhs station_config
    \return bool - true=equal
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool CWiFiESP::sta_config_equal(const wifi_config_t &lhs, const wifi_config_t &rhs) {
    if (memcmp(&lhs, &rhs, sizeof(wifi_config_t)) != 0) {
        return false;
    }
    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     wifi_sta_config

    \brief  Set the STA Config
    \param  N/A
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CWiFiESP::wifi_sta_config(wifi_config_t *wifi_config, const char *ssid, const char *password,
                               const uint8_t *bssid, uint8_t channel,
                               wifi_scan_method_t scan_method, wifi_sort_method_t sort_method,
                               uint16_t listen_interval, bool pmf_required) {
    wifi_config->sta.channel = channel;
    wifi_config->sta.listen_interval = listen_interval;
    wifi_config->sta.scan_method = scan_method; // WIFI_ALL_CHANNEL_SCAN or WIFI_FAST_SCAN
    wifi_config->sta.sort_method =
        sort_method; // WIFI_CONNECT_AP_BY_SIGNAL or WIFI_CONNECT_AP_BY_SECURITY
    wifi_config->sta.threshold.rssi = -127;
    wifi_config->sta.pmf_cfg.capable = true;
    wifi_config->sta.pmf_cfg.required = pmf_required;
    wifi_config->sta.bssid_set = 0;
    memset(wifi_config->sta.bssid, 0, 6);
    wifi_config->sta.threshold.authmode = WIFI_AUTH_OPEN;
    wifi_config->sta.ssid[0] = 0;
    wifi_config->sta.password[0] = 0;
    if (ssid != NULL && ssid[0] != 0) {
        _wifi_strncpy((char *)wifi_config->sta.ssid, ssid, 32);
        if (password != NULL && password[0] != 0) {
            wifi_config->sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            _wifi_strncpy((char *)wifi_config->sta.password, password, 64);
        }
        if (bssid != NULL) {
            wifi_config->sta.bssid_set = 1;
            memcpy(wifi_config->sta.bssid, bssid, 6);
        }
    }
}
