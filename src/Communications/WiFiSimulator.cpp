#if SIMULATOR

#include <string>

#include "SindarinWiFi.hpp"
#include "sindarin-debug.h"


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
CSindarinWiFi::CSindarinWiFi()
{
    // Base init variables
    m_nWiFiPower = WIFI_POWER_19_5dBm;
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
    strncpy((char *)conf->sta.ssid, "ssid", sizeof(conf->sta.ssid));
    strncpy((char *)conf->sta.password, "password", sizeof(conf->sta.password));
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
    return true;
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
    log_i("Pretending to connect WiFi (power %d)", WiFiPower);
    delay(1000);
    return true;

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
    log_i("Pretending to connect WiFi (power %d)", WiFiPower);
    delay(1000);
    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     DisconnectWifi

    \brief  Disconnect from WiFi
    \param  N/A
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CSindarinWiFi::DisconnectWifi() {
    log_i("Pretending to disconnect WiFi"); 
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
    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     ConnectWifi

    \brief  Connect via WiFi with a timeout to stop trying
    \param  timoutMS - timeout time in milliseconds
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::ConnectWifi(int timeoutMs) -> bool {
     log_i("Pretending to connect WiFi (power %d)", WiFiPower);
    delay(1000);
    return true;
}















#if OLD_WIFI

wifi_power_t WiFiPower = WIFI_POWER_19_5dBm;

auto GetWiFiCredentials(wifi_config_t *conf) -> bool {
    strncpy((char *)conf->sta.ssid, "ssid", sizeof(conf->sta.ssid));
    strncpy((char *)conf->sta.password, "password", sizeof(conf->sta.password));
    return true;
}


auto HasWiFiCredentials() -> bool { return true; }

auto ConnectWiFiIfCredentialsExist() -> bool {
    log_i("Pretending to connect WiFi (power %d)", WiFiPower);
    delay(1000);
    return true;
}

void DisconnectWifi() { log_i("Pretending to disconnect WiFi"); }

auto Connected() -> bool { return true; }

auto ConnectWifi(int timeoutMs) -> bool {
    log_i("Pretending to connect WiFi (power %d)", WiFiPower);
    delay(1000);
    return true;
}

void SetWiFiPower(wifi_power_t power) { WiFiPower = power; }

auto GetWiFiPower() -> wifi_power_t { return WiFiPower; }
#endif

#endif
