/**
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <freertos/FreeRTOS.h>
#include <esp_err.h>

#define ESP_AGENT_API_ENDPOINT CONFIG_ESP_AGENT_API_ENDPOINT

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Agent handle, to be passed for performing any action wrt agent.
 */
typedef void *esp_agent_handle_t;

/**
 * @brief Conversation types for initializing the conversation.
 */
typedef enum {
    ESP_AGENT_CONVERSATION_TEXT,
    ESP_AGENT_CONVERSATION_SPEECH,
    ESP_AGENT_CONVERSATION_TYPE_MAX,
} esp_agent_conversation_type_t;

/**
 * @brief Audio format for the conversation.
 */
typedef enum {
    ESP_AGENT_CONVERSATION_AUDIO_FORMAT_PCM,
    ESP_AGENT_CONVERSATION_AUDIO_FORMAT_OPUS,
    ESP_AGENT_CONVERSATION_AUDIO_FORMAT_MAX,
} esp_agent_conversation_audio_format_t;

/**
 * @brief Audio configuration for the conversation.
 */
typedef struct {
    esp_agent_conversation_audio_format_t format;
    uint16_t sample_rate;       /**< Sample rate in Hz (e.g., 8000, 16000) */
    uint8_t frame_duration;     /**< Frame duration in ms (e.g., 20, 40, 60) */
} esp_agent_audio_config_t;

/**
 * @brief Configuration for the agent.
 *
 * Relevant fields will be `strdup`-ed or `memcpy`-ed by `esp_agent_init`
 * so caller is free to deallocate any memory this requires.
 */
typedef struct {
    const char *agent_id;
    const char *refresh_token;
    esp_agent_conversation_type_t conversation_type;
    esp_agent_audio_config_t *upload_audio_config;
    esp_agent_audio_config_t *download_audio_config;
} esp_agent_config_t;

/**
 * @brief This will initialize the websocket client and internal variables.
 * Websocket will not be connected until `esp_agent_start` is called.
 *
 * @note agent_id and refresh_token in config are optional. They can be set later
 *       using esp_agent_set_agent_id() and esp_agent_set_refresh_token().
 *       However, both must be set before calling esp_agent_start().
 *
 * @param[in] config Pointer to agent configuration structure
 * @return Agent handle on success, NULL on failure
 */
esp_agent_handle_t esp_agent_init(const esp_agent_config_t *config);

/**
 * @brief This will deinitialize the websocket client and free the internal variables.
 *
 * @param[in] handle Agent handle obtained from esp_agent_init
 */
void esp_agent_deinit(esp_agent_handle_t handle);

/**
 * @brief Starts the websocket client(connects to the server)
 * And performs the handshake, thus starting the conversation.
 *
 * @param[in] handle Agent handle obtained from esp_agent_init
 * @param[in] conversation_id Optional conversation ID to resume a previous conversation. Pass NULL to start a new conversation.
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_agent_start(esp_agent_handle_t handle, const char *conversation_id);

/**
 * @brief This will stop the conversation and disconnect the websocket client.
 *
 * @param[in] handle Agent handle obtained from esp_agent_init
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_agent_stop(esp_agent_handle_t handle);

/**
 * @brief Sets the agent ID for the agent.
 *
 * @note If the agent is currently started, it will be stopped and restarted
 *       with the new agent ID. If the agent is not started, it will remain stopped.
 *
 * @param[in] handle Agent handle obtained from esp_agent_init
 * @param[in] agent_id Agent ID to set
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_agent_set_agent_id(esp_agent_handle_t handle, const char *agent_id);

/**
 * @brief Sets the refresh token for the agent.
 *
 * @param[in] handle Agent handle obtained from esp_agent_init
 * @param[in] refresh_token Refresh token to set
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_agent_set_refresh_token(esp_agent_handle_t handle, const char *refresh_token);

#ifdef __cplusplus
}
#endif
