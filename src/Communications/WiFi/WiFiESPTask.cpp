//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \file       WiFiESP.cpp

    \brief      WiFi ESP32 Wireless driver RTOS Task and Events

    Copyright (c) 2023 Sindarin, Inc. - All Right Reserved

    The code contained in this file is confidential and may only be used
    under license and/or written consent of Sindarin.
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "WiFiESPTask.hpp"			// File Header
#include "WiFiESP.hpp"              // ESP Class
#include "config.h"
#include "sindarin-debug.h"
#include "sdkconfig.h"
// Std Libs
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <functional>
#include <vector>
// Base Wifi
#include "esp_wifi.h"
#include "IPAddress.h"
#include "IPv6Address.h"
#include "esp_smartconfig.h"
#include "wifi_provisioning/manager.h"
// ESP Libs
#include "esp32-hal.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_event.h"
// RTOS Lib
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

// Define the Wifi Events
ESP_EVENT_DEFINE_BASE(WIFIESP_TASK_EVENTS);


//-----------------------------------------------------------------------------
// Statics
//-----------------------------------------------------------------------------
// Task Handler
static TaskHandle_t 		_wifiesp_event_task_handle = NULL;
// Event Queue
static xQueueHandle 		_wifiesp_event_queue;
// Event Group
static EventGroupHandle_t 	_wifiesp_event_group = NULL;


//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     GetAuthModeStr

    \brief  Get the string for the authentication mode
    \param  authmode - mode
    \return const char * - name
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
static const char * GetAuthModeStr(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
    	return ("OPEN");
        break;
    case WIFI_AUTH_WEP:
    	return ("WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
    	return ("PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
    	return ("WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
    	return ("WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
    	return ("WPA2_ENTERPRISE");
        break;
    default:
        break;
    }
	return ("UNKNOWN");
}


//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     WiFiESPTask_PostEvent

    \brief  Post event to wifi task
    \param  N/A
    \return N/A
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
esp_err_t WiFiESPTask_PostEvent(wifiesp_event_t *data)
{
	if(data == NULL){
        return ESP_FAIL;
	}
	wifiesp_event_t * event = (wifiesp_event_t*)malloc(sizeof(wifiesp_event_t));
	if(event == NULL){
        log_e("WiFi ESP Event Malloc Failed!");
        return ESP_FAIL;
	}
	memcpy(event, data, sizeof(wifiesp_event_t));
    if (xQueueSend(_wifiesp_event_queue, &event, portMAX_DELAY) != pdPASS) {
        log_e("WiFi ESP Event Send Failed!");
        return ESP_FAIL;
    }
    return ESP_OK;
}


//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     WiFiESPTask_EventTask

    \brief  The wifi event task
    \param  N/A
    \return N/A
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
static void WiFiESPTask_EventTask(void * arg){
	wifiesp_event_t *data = NULL;
    for (;;) {
        if(xQueueReceive(_wifiesp_event_queue, &data, portMAX_DELAY) == pdTRUE){
            CWiFiESP::_eventCallback(data);
            free(data);
            data = NULL;
        }
    }
    vTaskDelete(NULL);
    _wifiesp_event_task_handle = NULL;
}


//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     WiFiESPTask_EventCB

    \brief  Event Callback
    \param  N/A
    \return N/A
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
static void WiFiESPTask_EventCB(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	wifiesp_event_t wifiesp_event;
	wifiesp_event.event_id = WIFIESP_EVENT_MAX;

	/*
	 * STA
	 * */
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    	log_v("STA Started");
    	wifiesp_event.event_id = WIFIESP_EVENT_WIFI_STA_START;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP) {
    	log_v("STA Stopped");
    	wifiesp_event.event_id = WIFIESP_EVENT_WIFI_STA_STOP;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_AUTHMODE_CHANGE) {
    	wifi_event_sta_authmode_change_t * event = (wifi_event_sta_authmode_change_t*)event_data;
    	log_v("STA Auth Mode Changed: From: %s, To: %s", GetAuthModeStr(event->old_mode), GetAuthModeStr(event->new_mode));
    	wifiesp_event.event_id = WIFIESP_EVENT_WIFI_STA_AUTHMODE_CHANGE;
    	memcpy(&wifiesp_event.event_info.wifi_sta_authmode_change, event_data, sizeof(wifi_event_sta_authmode_change_t));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    	wifi_event_sta_connected_t * event = (wifi_event_sta_connected_t*)event_data;
    	log_v("STA Connected: SSID: %s, BSSID: " MACSTR ", Channel: %u, Auth: %s", event->ssid, MAC2STR(event->bssid), event->channel, GetAuthModeStr(event->authmode));
    	wifiesp_event.event_id = WIFIESP_EVENT_WIFI_STA_CONNECTED;
    	memcpy(&wifiesp_event.event_info.wifi_sta_connected, event_data, sizeof(wifi_event_sta_connected_t));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    	wifi_event_sta_disconnected_t * event = (wifi_event_sta_disconnected_t*)event_data;
    	log_v("STA Disconnected: SSID: %s, BSSID: " MACSTR ", Reason: %u", event->ssid, MAC2STR(event->bssid), event->reason);
    	wifiesp_event.event_id = WIFIESP_EVENT_WIFI_STA_DISCONNECTED;
    	memcpy(&wifiesp_event.event_info.wifi_sta_disconnected, event_data, sizeof(wifi_event_sta_disconnected_t));
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        log_v("STA Got %sIP:" IPSTR, event->ip_changed?"New ":"Same ", IP2STR(&event->ip_info.ip));
    	wifiesp_event.event_id = WIFIESP_EVENT_WIFI_STA_GOT_IP;
    	memcpy(&wifiesp_event.event_info.got_ip, event_data, sizeof(ip_event_got_ip_t));
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
    	log_v("STA IP Lost");
    	wifiesp_event.event_id = WIFIESP_EVENT_WIFI_STA_LOST_IP;

	/*
	 * SCAN
	 * */
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
    	wifi_event_sta_scan_done_t * event = (wifi_event_sta_scan_done_t*)event_data;
    	log_v("SCAN Done: ID: %u, Status: %u, Results: %u", event->scan_id, event->status, event->number);
    	wifiesp_event.event_id = WIFIESP_EVENT_WIFI_SCAN_DONE;
    	memcpy(&wifiesp_event.event_info.wifi_scan_done, event_data, sizeof(wifi_event_sta_scan_done_t));

	/*
	 * AP
	 * */
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
		log_v("AP Started");
    	wifiesp_event.event_id = WIFIESP_EVENT_WIFI_AP_START;
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) {
		log_v("AP Stopped");
    	wifiesp_event.event_id = WIFIESP_EVENT_WIFI_AP_STOP;
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_PROBEREQRECVED) {
		wifi_event_ap_probe_req_rx_t * event = (wifi_event_ap_probe_req_rx_t*)event_data;
		log_v("AP Probe Request: RSSI: %d, MAC: " MACSTR, event->rssi, MAC2STR(event->mac));
    	wifiesp_event.event_id = WIFIESP_EVENT_WIFI_AP_PROBEREQRECVED;
    	memcpy(&wifiesp_event.event_info.wifi_ap_probereqrecved, event_data, sizeof(wifi_event_ap_probe_req_rx_t));
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
		wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
		log_v("AP Station Connected: MAC: " MACSTR ", AID: %d", MAC2STR(event->mac), event->aid);
    	wifiesp_event.event_id = WIFIESP_EVENT_WIFI_AP_STACONNECTED;
    	memcpy(&wifiesp_event.event_info.wifi_ap_staconnected, event_data, sizeof(wifi_event_ap_staconnected_t));
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
		wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
		log_v("AP Station Disconnected: MAC: " MACSTR ", AID: %d", MAC2STR(event->mac), event->aid);
    	wifiesp_event.event_id = WIFIESP_EVENT_WIFI_AP_STADISCONNECTED;
    	memcpy(&wifiesp_event.event_info.wifi_ap_stadisconnected, event_data, sizeof(wifi_event_ap_stadisconnected_t));
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED) {
    	ip_event_ap_staipassigned_t * event = (ip_event_ap_staipassigned_t*)event_data;
    	log_v("AP Station IP Assigned:" IPSTR, IP2STR(&event->ip));
    	wifiesp_event.event_id = WIFIESP_EVENT_WIFI_AP_STAIPASSIGNED;
    	memcpy(&wifiesp_event.event_info.wifi_ap_staipassigned, event_data, sizeof(ip_event_ap_staipassigned_t));

	/*
	 * ETH
	 * */
	} else if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_CONNECTED) {
		log_v("Ethernet Link Up");
	    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
		(void)eth_handle;
    	wifiesp_event.event_id = WIFIESP_EVENT_ETH_CONNECTED;
    	memcpy(&wifiesp_event.event_info.eth_connected, event_data, sizeof(esp_eth_handle_t));
	} else if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_DISCONNECTED) {
		log_v("Ethernet Link Down");
    	wifiesp_event.event_id = WIFIESP_EVENT_ETH_DISCONNECTED;
	} else if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_START) {
		log_v("Ethernet Started");
    	wifiesp_event.event_id = WIFIESP_EVENT_ETH_START;
	} else if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_STOP) {
		log_v("Ethernet Stopped");
    	wifiesp_event.event_id = WIFIESP_EVENT_ETH_STOP;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        log_v("Ethernet got %sip:" IPSTR, event->ip_changed?"new":"", IP2STR(&event->ip_info.ip));
    	wifiesp_event.event_id = WIFIESP_EVENT_ETH_GOT_IP;
    	memcpy(&wifiesp_event.event_info.got_ip, event_data, sizeof(ip_event_got_ip_t));

	/*
	 * IPv6
	 * */
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_GOT_IP6) {
    	ip_event_got_ip6_t * event = (ip_event_got_ip6_t*)event_data;
    	esp_interface_t iface = CWiFiESP::get_esp_netif_interface(event->esp_netif);
    	log_v("IF[%d] Got IPv6: IP Index: %d, Zone: %d, " IPV6STR, iface, event->ip_index, event->ip6_info.ip.zone, IPV62STR(event->ip6_info.ip));
    	memcpy(&wifiesp_event.event_info.got_ip6, event_data, sizeof(ip_event_got_ip6_t));
    	if(iface == ESP_IF_WIFI_STA){
        	wifiesp_event.event_id = WIFIESP_EVENT_WIFI_STA_GOT_IP6;
    	} else if(iface == ESP_IF_WIFI_AP){
        	wifiesp_event.event_id = WIFIESP_EVENT_WIFI_AP_GOT_IP6;
    	} else if(iface == ESP_IF_ETH){
        	wifiesp_event.event_id = WIFIESP_EVENT_ETH_GOT_IP6;
    	}

	/*
	 * WPS
	 * */
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_WPS_ER_SUCCESS) {
    	wifiesp_event.event_id = WIFIESP_EVENT_WPS_ER_SUCCESS;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_WPS_ER_FAILED) {
    	wifi_event_sta_wps_fail_reason_t * event = (wifi_event_sta_wps_fail_reason_t*)event_data;
		(void)event;
		wifiesp_event.event_id = WIFIESP_EVENT_WPS_ER_FAILED;
    	memcpy(&wifiesp_event.event_info.wps_fail_reason, event_data, sizeof(wifi_event_sta_wps_fail_reason_t));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_WPS_ER_TIMEOUT) {
    	wifiesp_event.event_id = WIFIESP_EVENT_WPS_ER_TIMEOUT;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_WPS_ER_PIN) {
    	wifi_event_sta_wps_er_pin_t * event = (wifi_event_sta_wps_er_pin_t*)event_data;
		(void)event;
    	wifiesp_event.event_id = WIFIESP_EVENT_WPS_ER_PIN;
    	memcpy(&wifiesp_event.event_info.wps_er_pin, event_data, sizeof(wifi_event_sta_wps_er_pin_t));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP) {
    	wifiesp_event.event_id = WIFIESP_EVENT_WPS_ER_PBC_OVERLAP;

	/*
	 * FTM
	 * */
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_FTM_REPORT) {
    	wifi_event_ftm_report_t * event = (wifi_event_ftm_report_t*)event_data;
		(void)event;
    	wifiesp_event.event_id = WIFIESP_EVENT_WIFI_FTM_REPORT;
    	memcpy(&wifiesp_event.event_info.wifi_ftm_report, event_data, sizeof(wifi_event_ftm_report_t));


	/*
	 * SMART CONFIG
	 * */
	} else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
		log_v("SC Scan Done");
    	wifiesp_event.event_id = WIFIESP_EVENT_SC_SCAN_DONE;
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
    	log_v("SC Found Channel");
    	wifiesp_event.event_id = WIFIESP_EVENT_SC_FOUND_CHANNEL;
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        smartconfig_event_got_ssid_pswd_t *event = (smartconfig_event_got_ssid_pswd_t *)event_data;
        log_v("SC: SSID: %s, Password: %s", (const char *)event->ssid, (const char *)event->password);
    	wifiesp_event.event_id = WIFIESP_EVENT_SC_GOT_SSID_PSWD;
    	memcpy(&wifiesp_event.event_info.sc_got_ssid_pswd, event_data, sizeof(smartconfig_event_got_ssid_pswd_t));

    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
    	log_v("SC Send Ack Done");
    	wifiesp_event.event_id = WIFIESP_EVENT_SC_SEND_ACK_DONE;

	/*
	 * Provisioning
	 * */
	} else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_INIT) {
		log_v("Provisioning Initialized!");
    	wifiesp_event.event_id = WIFIESP_EVENT_PROV_INIT;
	} else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_DEINIT) {
		log_v("Provisioning Uninitialized!");
    	wifiesp_event.event_id = WIFIESP_EVENT_PROV_DEINIT;
	} else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_START) {
		log_v("Provisioning Start!");
    	wifiesp_event.event_id = WIFIESP_EVENT_PROV_START;
	} else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_END) {
		log_v("Provisioning End!");
		wifi_prov_mgr_deinit();
    	wifiesp_event.event_id = WIFIESP_EVENT_PROV_END;
	} else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_CRED_RECV) {
        wifi_sta_config_t *event = (wifi_sta_config_t *)event_data;
        log_v("Provisioned Credentials: SSID: %s, Password: %s", (const char *) event->ssid, (const char *) event->password);
    	wifiesp_event.event_id = WIFIESP_EVENT_PROV_CRED_RECV;
    	memcpy(&wifiesp_event.event_info.prov_cred_recv, event_data, sizeof(wifi_sta_config_t));
	} else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_CRED_FAIL) {
        wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
        log_e("Provisioning Failed: Reason : %s", (*reason == WIFI_PROV_STA_AUTH_ERROR)?"Authentication Failed":"AP Not Found");
    	wifiesp_event.event_id = WIFIESP_EVENT_PROV_CRED_FAIL;
    	memcpy(&wifiesp_event.event_info.prov_fail_reason, event_data, sizeof(wifi_prov_sta_fail_reason_t));
	} else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_CRED_SUCCESS) {
		log_v("Provisioning Success!");
    	wifiesp_event.event_id = WIFIESP_EVENT_PROV_CRED_SUCCESS;
    }
    
	if(wifiesp_event.event_id < WIFIESP_EVENT_MAX){
		WiFiESPTask_PostEvent(&wifiesp_event);
	}
}


//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     WiFiESPTask_StartNetworkEventTask

    \brief  Start the Network even task
    \param  N/A
    \return N/A
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
bool WiFiESPTask_StartNetworkEventTask(){
    if(!_wifiesp_event_group){
        _wifiesp_event_group = xEventGroupCreate();
        if(!_wifiesp_event_group){
            log_e("Network Event Group Create Failed!");
            return false;
        }
        xEventGroupSetBits(_wifiesp_event_group, WIFIESP_EVENTSTATUSBIT_WIFI_DNS_IDLE);
    }
    if(!_wifiesp_event_queue){
    	_wifiesp_event_queue = xQueueCreate(32, sizeof(wifiesp_event_t*));
        if(!_wifiesp_event_queue){
            log_e("Network Event Queue Create Failed!");
            return false;
        }
    }

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    	log_e("esp_event_loop_create_default failed!");
        return err;
    }

    if(!_wifiesp_event_task_handle){
        xTaskCreateUniversal(WiFiESPTask_EventTask, "wifiesp_events", WIFIESP_TASK_STACKSIZE, NULL, ESP_TASKD_EVENT_PRIO - 1, &_wifiesp_event_task_handle, WIFIESP_TASK_CORENUMBER);
        if(!_wifiesp_event_task_handle){
            log_e("Network Event Task Start Failed!");
            return false;
        }
    }

    if(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiESPTask_EventCB, NULL, NULL)){
        log_e("event_handler_instance_register for WIFI_EVENT Failed!");
        return false;
    }

    if(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &WiFiESPTask_EventCB, NULL, NULL)){
        log_e("event_handler_instance_register for IP_EVENT Failed!");
        return false;
    }

    if(esp_event_handler_instance_register(SC_EVENT, ESP_EVENT_ANY_ID, &WiFiESPTask_EventCB, NULL, NULL)){
        log_e("event_handler_instance_register for SC_EVENT Failed!");
        return false;
    }

    if(esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID, &WiFiESPTask_EventCB, NULL, NULL)){
        log_e("event_handler_instance_register for ETH_EVENT Failed!");
        return false;
    }

    if(esp_event_handler_instance_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &WiFiESPTask_EventCB, NULL, NULL)){
        log_e("event_handler_instance_register for WIFI_PROV_EVENT Failed!");
        return false;
    }

    return true;
}


//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     WiFiESPTask_SetEventStatusBits

    \brief  Set Event Status Bit
    \param  bits - bits to set
    \return int - status
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
int WiFiESPTask_SetEventStatusBits(int bits) 
{ 
    if(!_wifiesp_event_group){
        return 0;
    }
    return xEventGroupSetBits(_wifiesp_event_group, bits);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     WiFiESPTask_ClearEventStatusBits

    \brief  Clear Event Status Bit
    \param  bits - bits to clear
    \return int - status
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
int WiFiESPTask_ClearEventStatusBits(int bits){
    if(!_wifiesp_event_group){
        return 0;
    }
    return xEventGroupClearBits(_wifiesp_event_group, bits);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     WiFiESPTask_GetEventStatusBits

    \brief  Get Event Status Bit
    \param  N/A
    \return Current Bits Set
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
int WiFiESPTask_GetEventStatusBits(){
    if(!_wifiesp_event_group){
        return 0;
    }
    return xEventGroupGetBits(_wifiesp_event_group);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     WiFiESPTask_WaitEventStatusBits

    \brief  Wait for an Event Status Bit
    \param  bits - bits to wait for
    \param  timeout_ms - timeout in milliseconds
    \return int - status
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
int WiFiESPTask_WaitEventStatusBits(int bits, uint32_t timeout_ms){
    if(!_wifiesp_event_group){
        return 0;
    }
    return xEventGroupWaitBits(
        _wifiesp_event_group,    // The event group being tested.
        bits,  // The bits within the event group to wait for.
        pdFALSE,         // BIT_0 and BIT_4 should be cleared before returning.
        pdTRUE,        // Don't wait for both bits, either bit will do.
        timeout_ms / portTICK_PERIOD_MS ) & bits; // Wait a maximum of 100ms for either bit to be set.
}

