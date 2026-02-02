/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_log.h>
#include <cJSON.h>
#include <string.h>
#include <stdlib.h>

#include <esp_crt_bundle.h>
#include <esp_http_client.h>

#include <esp_agent_auth.h>
#include <esp_agent_internal.h>

#define USER_AUTH_TOKENS_PATH "/user/auth/tokens"

static const char *TAG = "esp_agent_auth";

/** Build HTTPS URL for /user/auth/tokens from API URL. */
static esp_err_t build_refresh_url(char **url_out)
{
    const char *api_url = esp_agents_get_api_endpoint();
    const char *path = USER_AUTH_TOKENS_PATH;

    const char *scheme = ESP_AGENT_API_USE_TLS ? "https" : "http";
    size_t scheme_len = strlen(scheme) + 3; /*  for "://" */

    size_t url_len = scheme_len + strlen(api_url) + strlen(path) + 1;
    *url_out = malloc(url_len);
    if (*url_out == NULL) {
        return ESP_ERR_NO_MEM;
    }
    snprintf(*url_out, url_len, "%s://%s%s", scheme, api_url, path);
    return ESP_OK;
}

esp_err_t esp_agent_auth_get_access_token(const char *refresh_token, char **access_token, size_t *access_token_len)
{
    if (!refresh_token || !access_token || !access_token_len) {
        ESP_LOGE(TAG, "Invalid parameters to fetch access token");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;
    esp_http_client_handle_t client = NULL;
    cJSON *json = NULL;
    cJSON *req_json = NULL;
    char *response_buffer = NULL;
    char *post_data = NULL;
    char *refresh_url = NULL;

    err = build_refresh_url(&refresh_url);
    if (err != ESP_OK) {
        goto end;
    }
    ESP_LOGD(TAG, "Refresh URL: %s", refresh_url);

    req_json = cJSON_CreateObject();
    if (req_json == NULL) {
        ESP_LOGE(TAG, "Failed to create request JSON");
        err = ESP_ERR_NO_MEM;
        goto end;
    }

    if (!cJSON_AddStringToObject(req_json, "refresh_token", refresh_token)) {
        ESP_LOGE(TAG, "Failed to add refresh_token to request");
        err = ESP_ERR_NO_MEM;
        goto end;
    }

    post_data = cJSON_PrintUnformatted(req_json);
    if (post_data == NULL) {
        ESP_LOGE(TAG, "Failed to serialize request JSON");
        err = ESP_ERR_NO_MEM;
        goto end;
    }

    cJSON_Delete(req_json);
    req_json = NULL;

    esp_http_client_config_t config = {
        .url = refresh_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 3072,
    };

    client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        err = ESP_FAIL;
        goto end;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    err = esp_http_client_open(client, strlen(post_data));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        goto end;
    }

    int wlen = esp_http_client_write(client, post_data, strlen(post_data));
    if (wlen < 0) {
        ESP_LOGE(TAG, "Failed to write POST data");
        err = ESP_FAIL;
        goto end;
    }
    ESP_LOGD(TAG, "Wrote %d bytes of POST data", wlen);

    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);

    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP request failed with status %d", status_code);
        err = ESP_ERR_INVALID_RESPONSE;
        goto end;
    }

    if (content_length <= 0) {
        ESP_LOGE(TAG, "Invalid or missing Content-Length: %d", content_length);
        err = ESP_ERR_INVALID_RESPONSE;
        goto end;
    }

    response_buffer = malloc((size_t)content_length + 1);
    if (response_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate response buffer (%d bytes)", content_length);
        err = ESP_ERR_NO_MEM;
        goto end;
    }

    int data_read = esp_http_client_read(client, response_buffer, content_length);
    if (data_read < content_length) {
        ESP_LOGE(TAG, "Read %d bytes, expected %d", data_read, content_length);
        err = ESP_FAIL;
        goto end;
    }

    response_buffer[content_length] = '\0';
    ESP_LOGD(TAG, "Response content: %s", response_buffer);

    json = cJSON_Parse(response_buffer);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        err = ESP_FAIL;
        goto end;
    }

    cJSON *access_token_json = cJSON_GetObjectItem(json, "access_token");
    if (access_token_json == NULL || !cJSON_IsString(access_token_json)) {
        ESP_LOGE(TAG, "Invalid access token in response");
        err = ESP_FAIL;
        goto end;
    }

    const char *token_value = cJSON_GetStringValue(access_token_json);
    size_t token_len = strlen(token_value);

    *access_token = malloc(token_len + 1);
    if (*access_token == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for access token");
        err = ESP_ERR_NO_MEM;
        goto end;
    }

    memcpy(*access_token, token_value, token_len + 1);
    (*access_token)[token_len] = '\0';
    *access_token_len = token_len;

    ESP_LOGI(TAG, "Successfully obtained access token (length: %zu)", token_len);

end:
    if(req_json) {
        cJSON_Delete(req_json);
    }
    if(json) {
        cJSON_Delete(json);
    }
    if(response_buffer) {
        free(response_buffer);
    }
    if (post_data) {
        cJSON_free(post_data);
    }
    if (refresh_url) {
        free(refresh_url);
    }
    if(client) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    return err;
}
