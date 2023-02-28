//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \file       SindarinWiFi.hpp

    \brief      WiFi wrapper for ESP32 Wireless driver Class Definition

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

#include "Communications/WiFi/WiFiESP.hpp"
#include "IPAddress.h"

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \class      CSindarinWiFi

    \brief      WiFi wrapper for ESP32 Wireless driver Class
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
class CSindarinWiFi {

public:
    /// Default Constructor
    CSindarinWiFi();

    // Credentials
    auto GetWiFiCredentials(wifi_config_t *conf) -> bool;
    auto HasWiFiCredentials() -> bool;

    // Scanning
    auto ScanNetworks(bool asyncScan) -> int16_t;
    auto ScanComplete() -> int16_t;
    auto GetSSID(uint8_t i) -> String;
    auto GetRSSI(uint8_t i) -> int32_t;
    auto GetEncryptionType(uint8_t i) -> wifi_auth_mode_t;

    // Connection
    void ConnectWiFi(const std::string &ssid = "", const std::string &password = "");
    auto ConnectWiFiIfCredentialsExist() -> bool;
    auto ConnectWiFiWithTimeout(int timeoutMs = 20000) -> bool;
    void DisconnectWiFi();
    auto Connected() -> bool;

    // Settings
    void SetWiFiPower(wifi_power_t power) { _wiFiPower = power; }
    auto GetWiFiPower() -> wifi_power_t { return _wiFiPower; }

    auto GetLocalIP() -> IPAddress;

    auto GetStatus() -> wifiesp_status_t;

    auto GetMode() -> wifi_mode_t;
    auto SetMode(wifi_mode_t mode) -> bool;

protected:
    //---------------------------------
    // Variables
    //---------------------------------
    wifi_power_t _wiFiPower = WIFI_POWER_19_5dBm;

    //---------------------------------
    // Functions
    //---------------------------------
};

// Global Object
extern CSindarinWiFi SindarinWiFi;
