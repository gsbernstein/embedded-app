//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \file       WiFiESP.hpp

    \brief      WiFi ESP32 Wireless driver Class Definition

    Copyright (c) 2023 Sindarin, Inc. - All Right Reserved

    The code contained in this file is confidential and may only be used
    under license and/or written consent of Sindarin.
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------

#pragma once

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <esp_wifi.h>
#include <functional>

#include "IPAddress.h"
#include "IPv6Address.h"
#include "WiFiESPDefs.hpp"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_smartconfig.h"
#include "wifi_provisioning/manager.h"

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \class      CWiFiESP

    \brief      WiFi wrapper for ESP32 Wireless driver Class
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
class CWiFiESP {

public:
    /// Default Constructor
    CWiFiESP();

    // Modes
    bool mode(wifi_mode_t);
    wifi_mode_t getMode();
    bool enableSTA(bool enable);

    // Scan
    int16_t scanNetworks(bool async = false, bool show_hidden = false, bool passive = false,
                         uint32_t max_ms_per_chan = 300, uint8_t channel = 0);
    static void _scanDoneCB();
    int16_t scanComplete();
    void scanDelete();
    // Scan Information
    void *getScanInfoByIndex(int i);
    bool getNetworkInfo(uint8_t networkItem, String &ssid, uint8_t &encryptionType, int32_t &RSSI,
                        uint8_t *&BSSID, int32_t &channel);
    String SSID(uint8_t networkItem);
    wifi_auth_mode_t encryptionType(uint8_t networkItem);
    int32_t RSSI(uint8_t networkItem);
    uint8_t *BSSID(uint8_t networkItem);
    String BSSIDstr(uint8_t networkItem);
    int32_t scan_channel(uint8_t networkItem);

    // Sleep
    bool setSleep(bool enabled);
    bool setSleep(wifi_ps_type_t sleepType);
    wifi_ps_type_t getSleep();

    // Power
    bool setTxPower(wifi_power_t power);
    wifi_power_t getTxPower();

    // Hostname
    const char *getHostname();
    bool setHostname(const char *hostname);
    bool hostname(const String &aHostname) { return setHostname(aHostname.c_str()); }

    // Start Wifi
    wifiesp_status_t begin(const char *wpa2_ssid, wifiesp_wpa2_auth_method_t method,
                           const char *wpa2_identity = NULL, const char *wpa2_username = NULL,
                           const char *wpa2_password = NULL, const char *ca_pem = NULL,
                           const char *client_crt = NULL, const char *client_key = NULL,
                           int32_t channel = 0, const uint8_t *bssid = 0, bool connect = true);
    wifiesp_status_t begin(const char *ssid, const char *passphrase = NULL, int32_t channel = 0,
                           const uint8_t *bssid = NULL, bool connect = true);
    wifiesp_status_t begin(char *ssid, char *passphrase = NULL, int32_t channel = 0,
                           const uint8_t *bssid = NULL, bool connect = true);
    wifiesp_status_t begin();

    // Auto Reconnect
    bool setAutoReconnect(bool autoReconnect);
    bool getAutoReconnect();

    // Connection
    bool reconnect();
    bool disconnect(bool wifioff = false, bool eraseap = false);
    bool isConnected();
    IPAddress localIP();

    // STA Status
    static void _setStatus(wifiesp_status_t status);
    static wifiesp_status_t status();

    // Event Callback
    static esp_err_t _eventCallback(wifiesp_event_t *event);
    // Events
    wifi_event_id_t onEvent(wifiesp_event_cb_t cbEvent,
                            wifiesp_event_id_t event = WIFIESP_EVENT_MAX);
    wifi_event_id_t onEvent(wifiesp_event_func_cb_t cbEvent,
                            wifiesp_event_id_t event = WIFIESP_EVENT_MAX);
    wifi_event_id_t onEvent(wifiesp_event_cb_sys_t cbEvent,
                            wifiesp_event_id_t event = WIFIESP_EVENT_MAX);
    void removeEvent(wifiesp_event_cb_t cbEvent, wifiesp_event_id_t event = WIFIESP_EVENT_MAX);
    void removeEvent(wifiesp_event_cb_sys_t cbEvent, wifiesp_event_id_t event = WIFIESP_EVENT_MAX);
    void removeEvent(wifi_event_id_t id);

    // Wait Status Bits
    static int waitEventStatusBits(int bits, uint32_t timeout_ms);
    // Get Network Inferace
    static esp_interface_t get_esp_netif_interface(esp_netif_t *esp_netif);

protected:
    //---------------------------------
    // Variables
    //---------------------------------

    // Init
    bool lowLevelInitDone = false;
    bool _esp_wifi_started = false;

    bool _persistent = true;
    bool _long_range = false;
    bool _autoReconnect = true;

    //---------------------------------
    // Functions
    //---------------------------------

    // Init
    bool tcpipInit();
    bool wifiLowLevelInit(bool persistent);
    bool wifiLowLevelDeinit();
    bool espWiFiStart();
    bool espWiFiStop();

    // Event Status Bits
    static int setEventStatusBits(int bits);
    static int clearEventStatusBits(int bits);
    static int getEventStatusBits();

    // Network Interface
    void add_esp_interface_netif(esp_interface_t interface, esp_netif_t *esp_netif);
    esp_netif_t *get_esp_interface_netif(esp_interface_t interface);
    esp_err_t set_esp_interface_hostname(esp_interface_t interface, const char *hostname);
    esp_err_t set_esp_interface_ip(esp_interface_t interface, IPAddress local_ip = IPAddress(),
                                   IPAddress gateway = IPAddress(), IPAddress subnet = IPAddress());
    esp_err_t set_esp_interface_dns(esp_interface_t interface, IPAddress main_dns = IPAddress(),
                                    IPAddress backup_dns = IPAddress(),
                                    IPAddress fallback_dns = IPAddress());
    static const char *get_esp_netif_hostname();
    static void set_esp_netif_hostname(const char *name);

    // Helper functions
    static size_t _wifi_strncpy(char *dst, const char *src, size_t dst_len);
    // STA Helpers
    static bool sta_config_equal(const wifi_config_t &lhs, const wifi_config_t &rhs);
    static void wifi_sta_config(wifi_config_t *wifi_config, const char *ssid = NULL,
                                const char *password = NULL, const uint8_t *bssid = NULL,
                                uint8_t channel = 0,
                                wifi_scan_method_t scan_method = WIFI_ALL_CHANNEL_SCAN,
                                wifi_sort_method_t sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
                                uint16_t listen_interval = 0, bool pmf_required = false);
};

//-----------------------------------------------------------------------------
// Extern Object
//-----------------------------------------------------------------------------
// Global Object
extern CWiFiESP WiFiESP;
