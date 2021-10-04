/**
 * @file wifiStation.cpp
 * @author Eddy Beaupré (https://github.com/EddyBeaupre)
 * @brief ESP32 WiFi Station Library
 * @version 1.1.0
 * @date 2021-10-03
 * 
 * @copyright Copyright 2021 Eddy Beaupré <eddy@beaupre.biz>
 *            Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 *            following conditions are met:
 * 
 *            1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
 *               disclaimer.
 * 
 *            2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
 *               following disclaimer in the documentation and/or other materials provided with the distribution.
 * 
 *            THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS  "AS IS"  AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *            INCLUDING,   BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *            DISCLAIMED.   IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *            SPECIAL,  EXEMPLARY,  OR CONSEQUENTIAL DAMAGES  (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS OR
 *            SERVICES;  LOSS OF USE, DATA, OR PROFITS;  OR BUSINESS INTERRUPTION)  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *            WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *            OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include <sdkconfig.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_wifi.h>
#include <esp_log.h>

#include "wifiStation.hpp"

#define LOGI(v, t, f, ...)             \
    if (v)                             \
    {                                  \
        ESP_LOGI(t, f, ##__VA_ARGS__); \
    }

/**
 * @brief Construct a new wifiStation object
 * 
 * @param ssid      SSID Name
 * @param password  SSID Password
 * @param hostName  Device's hostname
 */
wifiStation::wifiStation(const char *ssid, const char *password, const char *hostName, bool verbose)
{
    wifi_config_t wifiConfig;
    memset(&wifiConfig, 0, sizeof(wifi_config_t));

    memcpy(wifiConfig.sta.ssid, ssid, strlen(ssid) < sizeof(wifiConfig.sta.ssid) ? strlen(ssid) : sizeof(wifiConfig.sta.ssid));
    memcpy(wifiConfig.sta.password, password, strlen(password) < sizeof(wifiConfig.sta.password) ? strlen(password) : sizeof(wifiConfig.sta.password));
    wifiConfig.sta.pmf_cfg.capable = true;
    wifiConfig.sta.pmf_cfg.required = false;

    setup(wifiConfig, hostName, verbose);
}

/**
 * @brief Construct a new wifiStation object
 * 
 * @param wifiStaConfig     STA configuration settings
 * @param hostName          Device's hostname
 */
wifiStation::wifiStation(wifi_sta_config_t wifiStaConfig, const char *hostName, bool verbose)
{
    wifi_config_t wifiConfig;
    memset(&wifiConfig, 0, sizeof(wifi_config_t));
    memcpy(&wifiConfig.sta, &wifiStaConfig, sizeof(wifi_sta_config_t));
    setup(wifiConfig, hostName, verbose);
}

/**
 * @brief Construct a new wifiStation object
 * 
 * @param wifiConfig    WiFi configuration settings
 * @param hostname      Device's hostname
 * @param verbose       Verbose logging
 */
void wifiStation::setup(wifi_config_t wifiConfig, const char *hostname, bool verbose)
{
    verboseLoggin = verbose;

    if (hostName == NULL)
    {
        uint8_t mac[6] = {0};
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        asprintf(&hostName, "esp32-%02x%02x.%02x%02x.%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    else
    {
        hostName = strdup(hostname);
    }

    wifiSta = esp_netif_create_default_wifi_sta();
    assert(wifiSta);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, this->eventHandler, this, &wlEventHandlerInstance));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, this->eventHandler, this, &ipEventHandlerInstance));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/**
 * @brief Main wifi event handler
 * 
 * @param arg           wifiStation Instance
 * @param event_base    base event type
 * @param event_id      event ID
 * @param event_data    Pointer to event's data
 */
void wifiStation::eventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifiStation *Instance = (wifiStation *)arg;

    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_WIFI_READY:
            LOGI(Instance->verboseLoggin, "wifiStation::eventHandler", "WiFi ready");
            break;
        case WIFI_EVENT_SCAN_DONE:
            LOGI(Instance->verboseLoggin, "wifiStation::eventHandler", "Finish scanning AP");
            break;
        case WIFI_EVENT_STA_START:
            LOGI(Instance->verboseLoggin, "wifiStation::eventHandler", "Station start");
            ESP_ERROR_CHECK(esp_netif_set_hostname(Instance->wifiSta, Instance->hostName));
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_STOP:
            LOGI(Instance->verboseLoggin, "wifiStation::eventHandler", "Station stop");
            break;
        case WIFI_EVENT_STA_CONNECTED:
            LOGI(Instance->verboseLoggin, "wifiStation::eventHandler", "Connected to AP");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            LOGI(Instance->verboseLoggin, "wifiStation::eventHandler", "Disconnected from AP");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_AUTHMODE_CHANGE:
            LOGI(Instance->verboseLoggin, "wifiStation::eventHandler", "Auth mode of AP connected by station changed");
            break;
        default:
            LOGI(Instance->verboseLoggin, "wifiStation::eventHandler", "Unhandled WiFi Event");
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
            LOGI(Instance->verboseLoggin, "wifiStation::eventHandler", "station got IP from connected AP");
            break;
        case IP_EVENT_STA_LOST_IP:
            LOGI(Instance->verboseLoggin, "wifiStation::eventHandler", "Station lost IP");
            break;
        case IP_EVENT_GOT_IP6:
            ESP_LOGI("wifiStation::eventHandler", "Station interface v6IP addr is preferred");
            break;
        default:
            LOGI(Instance->verboseLoggin, "wifiStation::eventHandler", "Unhandled IP Event");
            break;
        }
    }
}

/**
 * @brief Return information of AP which the ESP32 station is associated with
 * 
 * @return wifi_ap_record_t information of the WiFi AP
 */
wifi_ap_record_t wifiStation::getApInfo(void)
{
    wifi_ap_record_t ap_info;

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_sta_get_ap_info(&ap_info));

    return (ap_info);
}

/**
 * @brief Return the network interface descriptor.
 * 
 * @return esp_netif_t* 
 */
esp_netif_t *wifiStation::getNetIf(void)
{
    return wifiSta;
}

wifiStation::~wifiStation(void) {
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wlEventHandlerInstance));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ipEventHandlerInstance));
    free(hostName);
    wlEventHandlerInstance = NULL;
    ipEventHandlerInstance = NULL;
    hostName = NULL;
}