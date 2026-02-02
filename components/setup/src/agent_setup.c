/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_check.h>
#include <nvs_flash.h>
#include <string.h>

#include <esp_wifi.h>
#include <esp_netif.h>
#include <app_network.h>

#include <agent_console.h>

#include "agent_setup.h"
#include "setup/rainmaker.h"
#include "setup/console.h"

#define AGENT_SETUP_NVS_NAMESPACE "agent_setup"
#define AGENT_SETUP_NVS_KEY_REFRESH_TOKEN "refresh_token"
#define AGENT_SETUP_NVS_KEY_AGENT_ID "agent_id"

typedef struct {
    bool init_done;
    char *agent_id;
    char *refresh_token;
    struct {
        uint8_t network_connected: 1;
        uint8_t agent_id_set: 1;
        uint8_t refresh_token_set: 1;
    } flags;
} agent_setup_data_t;

static agent_setup_data_t g_agent_setup_data;
const char *TAG = "agent_setup";

ESP_EVENT_DEFINE_BASE(AGENT_SETUP_EVENT);

static void start_task(void *pvParameters)
{
    while (1) {
        if (g_agent_setup_data.flags.network_connected &&
            g_agent_setup_data.flags.agent_id_set &&
            g_agent_setup_data.flags.refresh_token_set) {

            ESP_LOGI(TAG, "Posting AGENT_SETUP_EVENT_START event");
            esp_event_post(AGENT_SETUP_EVENT, AGENT_SETUP_EVENT_START, NULL, 0, portMAX_DELAY);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(NULL);
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base != IP_EVENT) {
        return;
    }

    if (event_id != IP_EVENT_STA_GOT_IP) {
        return;
    }

    ESP_LOGD(TAG, "Network Connected");
    g_agent_setup_data.flags.network_connected = true;
    esp_event_post(AGENT_SETUP_EVENT, AGENT_SETUP_EVENT_NETWORK_CONNECTED, NULL, 0, portMAX_DELAY);
}

static int set_wifi_cli_handler(int argc, char *argv[])
{
    if (argc != 3) {
        ESP_LOGE(TAG, "Incorrect arguments\nUsage: set-wifi <ssid> <password>");
        return 0;
    }

    /**
     * Initialize WiFi with default config
     * This is ignored internally if already inited
     */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init WiFi");
        return 0;
    }

    /* Stop WiFi */
    if (esp_wifi_stop() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop wifi");
    }

    /* Configure WiFi as station */
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode");
        return 0;
    }

    wifi_config_t wifi_cfg = {0};
    snprintf((char*)wifi_cfg.sta.ssid, sizeof(wifi_cfg.sta.ssid), "%s", argv[1]);
    snprintf((char*)wifi_cfg.sta.password, sizeof(wifi_cfg.sta.password), "%s", argv[2]);

    /* Configure WiFi station with provided host credentials */
    if (esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi configuration");
        return 0;
    }
    /* (Re)Start WiFi */
    if (esp_wifi_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi");
        return 0;
    }
    /* Connect to AP */
    if (esp_wifi_connect() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect WiFi");
        return 0;
    }

    return 0;
}

esp_err_t register_set_wifi_cli_handler(void)
{
    esp_console_cmd_t cmd = {
        .command = "set-wifi",
        .help = "Set WiFi credentials\nUsage: set-wifi <ssid> <password>",
        .func = set_wifi_cli_handler,
    };
    return agent_console_register_command(&cmd);
}

static esp_err_t save_str_to_nvs(const char *key, const char *value)
{
    esp_err_t err;
    nvs_handle_t handle;
    err = nvs_open(AGENT_SETUP_NVS_NAMESPACE, NVS_READWRITE, &handle);
    ESP_RETURN_ON_ERROR(err, TAG, "Failed to open namespace for writing: %s", AGENT_SETUP_NVS_NAMESPACE);

    err = nvs_set_str(handle, key, value);
    ESP_RETURN_ON_ERROR(err, TAG, "Failed to write to NVS: %s", key);

    ESP_RETURN_ON_ERROR(nvs_commit(handle), TAG, "Failed to commit NVS changes");

    nvs_close(handle);

    return ESP_OK;
}

static esp_err_t get_str_from_nvs(const char *key, char **value_buf, size_t *value_len)
{
    esp_err_t err;
    nvs_handle_t handle;

    char *buf = NULL;
    size_t read_len;

    err = nvs_open(AGENT_SETUP_NVS_NAMESPACE, NVS_READONLY, &handle);
    ESP_RETURN_ON_ERROR(err, TAG, "Failed to open namespace for reading: %s", AGENT_SETUP_NVS_NAMESPACE);

    err = nvs_get_str(handle, key, NULL, &read_len);
    ESP_RETURN_ON_ERROR(err, TAG, "Failed to get string length: %s", key);

    buf = malloc(read_len);
    ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for value");

    err = nvs_get_str(handle, key, buf, &read_len);
    ESP_RETURN_ON_ERROR(err, TAG, "Failed to get string: %s", key);

    nvs_close(handle);
    *value_buf = buf;
    *value_len = read_len;

    return ESP_OK;
}

static void get_default_agent_id(char **buf)
{
#ifdef CONFIG_AGENT_SETUP_DEFAULT_AGENT_ID
    if (CONFIG_AGENT_SETUP_DEFAULT_AGENT_ID && strlen(CONFIG_AGENT_SETUP_DEFAULT_AGENT_ID) > 0) {
        *buf = strdup(CONFIG_AGENT_SETUP_DEFAULT_AGENT_ID);
    }
#endif
}

esp_err_t agent_setup_init()
{
    if (g_agent_setup_data.init_done) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Register setup commands with console */
    setup_console_register_commands();

    char *buf = NULL;
    size_t buf_len = 0;

    esp_err_t ret;
    ret = get_str_from_nvs(AGENT_SETUP_NVS_KEY_REFRESH_TOKEN, &buf, &buf_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get Refresh Token from NVS");
    } else {
        ESP_LOGD(TAG, "Got Refresh Token from NVS: %s", buf);
        g_agent_setup_data.refresh_token = buf;
        g_agent_setup_data.flags.refresh_token_set = true;
    }

    buf = NULL;
    buf_len = 0;
    ret = get_str_from_nvs(AGENT_SETUP_NVS_KEY_AGENT_ID, &buf, &buf_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get Agent ID from NVS");
        buf = NULL;
        /* Use a fallback agent id if previous is not found in NVS */
        get_default_agent_id(&buf);
    }

    if (buf) {
        ESP_LOGI(TAG, "Using Agent ID: %s", buf);

        g_agent_setup_data.agent_id = buf;
        g_agent_setup_data.flags.agent_id_set = true;
    }


    ESP_GOTO_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL), err, TAG, "Failed to register event handler");

    /* Initialize network connection + provisioning */
    app_network_init();
    app_network_set_custom_mfg_data(MGF_DATA_DEVICE_TYPE_USER_AUTH, MFG_DATA_DEVICE_SUBTYPE_AI_AGENT);

    /** TODO: Change this */
    xTaskCreate(start_task, "start_task", 4*1024, NULL, 5, NULL);

    ESP_RETURN_ON_ERROR(register_set_wifi_cli_handler(), TAG, "Failed to register set-wifi CLI handler");

    g_agent_setup_data.init_done = true;

    return ESP_OK;

err:
    if (g_agent_setup_data.refresh_token) {
        free(g_agent_setup_data.refresh_token);
        g_agent_setup_data.refresh_token = NULL;
    }
    if (g_agent_setup_data.agent_id) {
        free(g_agent_setup_data.agent_id);
        g_agent_setup_data.agent_id = NULL;
    }
    return ret;
}

char* agent_setup_get_agent_id(void)
{
    if (!g_agent_setup_data.init_done) {
        ESP_LOGE(TAG, "Can't get agent id: not initialized");
        return NULL;
    }

    return g_agent_setup_data.agent_id;
}

char* agent_setup_get_refresh_token(void)
{
    if (!g_agent_setup_data.init_done) {
        ESP_LOGE(TAG, "Can't get refresh token: not initialized");
        return NULL;
    }

    return g_agent_setup_data.refresh_token;
}

esp_err_t agent_setup_set_agent_id(const char *agent_id)
{
    if (!g_agent_setup_data.init_done) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;

    /* So that we don't lose existing agent_id if allocation fails */
    char *agent_id_tmp = strdup(agent_id);
    if (agent_id_tmp == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for Agent ID");
        return ESP_ERR_NO_MEM;
    }

    ESP_GOTO_ON_ERROR(save_str_to_nvs(AGENT_SETUP_NVS_KEY_AGENT_ID, agent_id_tmp), err, TAG, "Failed to save Agent ID to NVS");

    if (g_agent_setup_data.agent_id) {
        free(g_agent_setup_data.agent_id);
        g_agent_setup_data.agent_id = NULL;
    }
    g_agent_setup_data.agent_id = agent_id_tmp;
    g_agent_setup_data.flags.agent_id_set = true;


    esp_event_post(AGENT_SETUP_EVENT, AGENT_SETUP_EVENT_AGENT_ID_UPDATE, NULL, 0, portMAX_DELAY);

    return ESP_OK;
err:
    if (agent_id_tmp) {
        free(agent_id_tmp);
        agent_id_tmp = NULL;
    }
    return ret;
}

esp_err_t agent_setup_set_refresh_token(const char *refresh_token)
{
    if (!g_agent_setup_data.init_done) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;

    /* Same reason as above */
    char *refresh_token_tmp = strdup(refresh_token);
    if (refresh_token_tmp == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for Refresh Token");
        return ESP_ERR_NO_MEM;
    }

    if (g_agent_setup_data.refresh_token) {
        free(g_agent_setup_data.refresh_token);
        g_agent_setup_data.refresh_token = NULL;
    }
    g_agent_setup_data.refresh_token = refresh_token_tmp;
    g_agent_setup_data.flags.refresh_token_set = true;

    ESP_LOGD(TAG, "Saving Refresh Token to NVS: %s", g_agent_setup_data.refresh_token);
    ESP_GOTO_ON_ERROR(save_str_to_nvs(AGENT_SETUP_NVS_KEY_REFRESH_TOKEN, g_agent_setup_data.refresh_token), err, TAG, "Failed to save Refresh Token to NVS");

    ESP_LOGD(TAG, "Refresh Token saved to NVS");

    return ESP_OK;
err:
    if (g_agent_setup_data.refresh_token) {
        free(g_agent_setup_data.refresh_token);
        g_agent_setup_data.refresh_token = NULL;
    }
    return ret;
}

esp_err_t agent_setup_start(void)
{
    if (!g_agent_setup_data.init_done) {
        ESP_LOGE(TAG, "Agent setup not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    return app_network_start(POP_TYPE_RANDOM);
}

esp_err_t agent_setup_factory_reset(void)
{
    return setup_rainmaker_factory_reset();
}
