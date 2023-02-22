//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \file       SindarinWiFi.cpp

    \brief      WiFi wrapper for ESP32 Wireless driver Class Implementation

    Copyright (c) 2023 Sindarin, Inc. - All Right Reserved

    The code contained in this file is confidential and may only be used
    under license and/or written consent of Sindarin.
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "Communications/SindarinWiFi.hpp"              // Class Header

#include <cstring>
#include "config.h"
#include "sindarin-debug.h"

/*
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
*/


// Create Object
CSindarinWiFi SindarinWiFi;


//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     CSindarinWiFi

    \brief  Constructor
    \param  N/A
    \return N/A
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
CSindarinWiFi::CSindarinWiFi() {
    // Base init variables
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     GetWiFiCredentials

    \brief  Get the current Wifi credentials
    \param  pConf - Return value - Pointer to the config to return into
    \return bool - true = success
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::GetWiFiCredentials(wifi_config_t *conf) -> bool {
    // Needs to be called before we can get the WiFi config
    if (!WiFi.enableSTA(true)) {
        log_e("STA enable failed!");
        return false;
    }

    if (esp_wifi_get_config(WIFI_IF_STA, conf) != ESP_OK) {
        log_e("Failed to get WiFi config");
        return false;
    }
    if (strlen((const char *)conf->sta.ssid) == 0) {
        return false;
    }
    if (strlen((const char *)conf->sta.password) == 0) {
        log_w("Zero length WiFi password");
    }
    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     HasWiFiCredentials

    \brief  Do we have Wifi credentials
    \param  N/A
    \return bool - true = have credentials
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::HasWiFiCredentials() -> bool {
    wifi_config_t conf;
    return GetWiFiCredentials(&conf);
}


//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     ScanNetworks

    \brief  Scan for available wifi networks
    \param  asyncScan - perform a asynch scan
    \return int16_t - number networks found
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::ScanNetworks(bool asyncScan) -> int16_t {
    return WiFi.scanNetworks(asyncScan);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     ScanComplete

    \brief  Called to get the scan in async mode
    \param  N/A
    \return int16_t - number networks found or negative = error
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::ScanComplete() -> int16_t {
    return WiFi.scanComplete();
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     GetSSID

    \brief  Get the SSID for a scanned network at provided offset
    \param  i - offset for scanned network
    \return String - the SSID
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::GetSSID(uint8_t i) -> String {
    return WiFi.SSID(i);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     GetRSSI

    \brief  Get the RSSI for a scanned network at provided offset
    \param  i - offset for scanned network
    \return int32_t - the RSSI
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::GetRSSI(uint8_t i) -> int32_t {
    return WiFi.RSSI(i);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     GetEncryptionType

    \brief  Get the Encryption for a scanned network at provided offset
    \param  i - offset for scanned network
    \return wifi_auth_mode_t - encryption type
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::GetEncryptionType(uint8_t i) -> wifi_auth_mode_t {
    return WiFi.encryptionType(i);
}


//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     ConnectWiFi

    \brief  Connect via WiFi
    \param  ssid - SSID to connect to
    \param  password - Password for the network
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CSindarinWiFi::ConnectWiFi(const std::string &ssid, const std::string &password) {
   // We have to have wifi sleep enabled otherwise Bluetooth can't work
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    // I'm not sure, but it might be that WiFi sleep before connecting to an AP could
    // be problematic.
    // if (!WiFi.setSleep(true)) {
    //     log_e("Failed to enable WiFi sleep");
    // }

    // If credentials are available, go ahead and attempt WiFi connection
    wifi_config_t conf;
    GetWiFiCredentials(&conf);
    log_i("Found WiFi credentials for %s. Attempting connection", (const char *)conf.sta.ssid);
    WiFi.setHostname(HOSTNAME);
    if (ssid.empty()) {
        WiFi.begin();
    } else {
        WiFi.begin(ssid.c_str(), password.c_str());
    }
    log_d("WiFi TX Power was %d", WiFi.getTxPower());
    WiFi.setTxPower(_wiFiPower);
    log_d("WiFi TX Power is now %d", WiFi.getTxPower());
    WiFi.setAutoReconnect(true);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     ConnectWiFiIfCredentialsExist

    \brief  Connect via WiFi if we have credentials
    \param  N/A
    \return bool - true = connected
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::ConnectWiFiIfCredentialsExist() -> bool {
    if (!HasWiFiCredentials()) {
        log_d("No WiFi credentials found");
        return false;
    }
    ConnectWiFi();
    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     ConnectWiFiWithTimeout

    \brief  Connect via WiFi with a timeout to stop trying
    \param  timoutMS - timeout time in milliseconds
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::ConnectWiFiWithTimeout(int timeoutMs) -> bool {
    auto startTime = millis();
    if (!Connected()) {
        if( ConnectWiFiIfCredentialsExist() ) {
            // Only wait if we had credentials
            // TODO: Would be nice to make this not a polling loop
            while (!Connected() && (millis() - startTime) < timeoutMs) {
                delay(200);
            }
        }
        if (!Connected()) {
            DisconnectWiFi();
            return false;
        }
    }

    return true;
}


//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     DisconnectWiFi

    \brief  Disconnect from WiFi
    \param  N/A
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CSindarinWiFi::DisconnectWiFi() {
    log_i("Disconnecting WiFi");
    WiFi.setAutoReconnect(false);
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     Connected

    \brief  Are we currently connected to network
    \param  N/A
    \return bool - true = connected
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::Connected() -> bool {
    return WiFi.isConnected();
}


//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     GetLocalIP

    \brief  Get the station interface IP address
    \param  N/A
    \return IPAddress = 0 if not cennected
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::GetLocalIP() -> IPAddress
{
    return WiFi.localIP();

/*
    IPAddress nAddr;
    nAddr.dword = 0;

    if(Connected())
    {
	    esp_netif_ip_info_t nEspIP;
        if(esp_netif_get_ip_info(get_esp_interface_netif(ESP_IF_WIFI_STA), &nEspIP) != ESP_OK){
    	    log_e("Netif Get IP Failed!");
    	    return nAddr;
    }
    // Got here must have gotten real ip
    nAppr.dword = nEspIP.ip.addr;
*/    
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     GetStatus

    \brief  Return Connection status
    \param  N/A
    \return wl_status_t
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
wl_status_t CSindarinWiFi::GetStatus()
{
    return WiFi.status();
/*    
    if(!_sta_status_group){
        return _sta_status;
    }
    return (wl_status_t)xEventGroupClearBits(_sta_status_group, 0);
*/    
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     GetMode

    \brief  Return Wifi mode
    \param  N/A
    \return wifi_mode_t
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::GetMode() -> wifi_mode_t
{
    return WiFi.getMode();
/*    
    wifi_mode_t mode;
    if(esp_wifi_get_mode(&mode) != ESP_OK){
        log_w("WiFi not started");
        return WIFI_MODE_NULL;
    }
    return mode;
*/    
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     SetMode

    \brief  Set a new mode
    \param  wifi_mode_t - mode to set
    \return bool - true= mode set
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool CSindarinWiFi::SetMode(wifi_mode_t mode)
{
    return WiFi.mode(mode);
/*
    wifi_mode_t cm = GetMode();
    if(cm == mode) {
        return true;
    }
    if(!cm && mode){
        if(!wifiLowLevelInit(_persistent)){
            return false;
        }
    } else if(cm && !mode){
        return espWiFiStop();
    }

    esp_err_t err;
    if(m & WIFI_MODE_STA){
    	err = set_esp_interface_hostname(ESP_IF_WIFI_STA, get_esp_netif_hostname());
        if(err){
            log_e("Could not set hostname! %d", err);
            return false;
        }
    }
    err = esp_wifi_set_mode(m);
    if(err){
        log_e("Could not set mode! %d", err);
        return false;
    }
    if(_long_range){
        if(m & WIFI_MODE_STA){
            err = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
            if(err != ESP_OK){
                log_e("Could not enable long range on STA! %d", err);
                return false;
            }
        }
        if(m & WIFI_MODE_AP){
            err = esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_LR);
            if(err != ESP_OK){
                log_e("Could not enable long range on AP! %d", err);
                return false;
            }
        }
    }
    if(!espWiFiStart()){
        return false;
    }

    return true;
*/    
}






//-----------------------------------------------------------------------------
// PRIVATE
//-----------------------------------------------------------------------------
