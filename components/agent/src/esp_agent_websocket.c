/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <string.h>
#include <stdlib.h>

#include <cJSON.h>

#include <esp_log.h>
#include <esp_event.h>
#include <esp_check.h>
#include <esp_websocket_client.h>

#include <esp_agent.h>
#include <esp_agent_core.h>
#include <esp_agent_internal.h>
#include <esp_agent_websocket.h>
#include <esp_agent_internal_messages.h>
#include <esp_agent_internal_events.h>
#include <esp_agent_auth.h>

static const char *TAG = "esp_agent_ws";

#define ACCESS_TOKEN_EXPIRATION_SECONDS 3600

void esp_agent_websocket_send_task(void *pvParameters)
{
    esp_agent_t *agent = (esp_agent_t *)pvParameters;
    ws_send_message_t *msg = NULL;
    int ws_ret = -1;
    esp_err_t ret = ESP_OK;
    ws_transport_opcodes_t send_opcode;

    ESP_LOGD(TAG, "WebSocket Send Task Started");

    /* Just to avoid compiler warning */
    if (ret) {}

    while (1) {
        /* Check for stop event first (non-blocking) */
        EventBits_t bits = xEventGroupGetBits(agent->event_group);
        if (bits & SEND_TASK_STOP_BIT) {
            ESP_LOGD(TAG, "WebSocket Send Task received stop signal, exiting");
            break;
        }

        /* Try to receive message with timeout */
        if (xQueueReceive(agent->send_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (msg == NULL) {
                continue;
            }

            ESP_GOTO_ON_FALSE(esp_websocket_client_is_connected(agent->ws_client), ESP_ERR_INVALID_STATE, deallocate_message, TAG, "WebSocket not connected, dropping message");

            switch (msg->type) {
                case WS_SEND_MSG_TYPE_TEXT:
                    send_opcode = WS_TRANSPORT_OPCODES_TEXT;
                    break;
                case WS_SEND_MSG_TYPE_BINARY:
                    send_opcode = WS_TRANSPORT_OPCODES_BINARY;
                    break;
                default:
                    goto deallocate_message;
            }

            ws_ret = esp_websocket_client_send_with_opcode(agent->ws_client, send_opcode, (const uint8_t *)msg->payload, msg->len, pdMS_TO_TICKS(5000));
            if (ws_ret < 0) {
                ESP_LOGE(TAG, "Failed to send message: %d", ws_ret);
            }

        deallocate_message:
            if (msg->payload) {
                free(msg->payload);
            }
            free(msg);
        }
    }

    ESP_LOGD(TAG, "WebSocket Send Task exiting cleanly");
    vTaskDelete(NULL);
}

esp_err_t esp_agent_websocket_queue_message(esp_agent_handle_t handle, ws_send_msg_type_t type, const char *payload, size_t len, TickType_t timeout)
{
    if (handle == NULL || payload == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_agent_t *agent = (esp_agent_t *)handle;

    if (!agent->started) {
        ESP_LOGW(TAG, "Agent not started, cannot queue message");
        return ESP_ERR_INVALID_STATE;
    }

    if (agent->send_queue == NULL) {
        ESP_LOGE(TAG, "Send queue not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ws_send_message_t *msg = malloc(sizeof(ws_send_message_t));
    if (msg == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for send message");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;

    msg->payload = malloc(len);
    ESP_GOTO_ON_FALSE(msg->payload, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate memory for payload");
    memcpy(msg->payload, payload, len);
    msg->type = type;
    msg->len = len;

    ESP_GOTO_ON_FALSE(xQueueSend(agent->send_queue, &msg, timeout), ESP_ERR_TIMEOUT, error, TAG, "Failed to queue message (queue full), dropping");

    ESP_LOGV(TAG, "Queued %s message: %d bytes", type == WS_SEND_MSG_TYPE_TEXT ? "text" : "binary", len);
    return ret;

error:
    if (msg->payload) {
        free(msg->payload);
    }
    if (msg) {
        free(msg);
    }
    return ret;
}

static esp_err_t build_ws_uri(const char *agent_id, const char *access_token, char **uri_out, size_t *uri_len)
{
    if (agent_id == NULL || access_token == NULL || uri_out == NULL || uri_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *api_url = esp_agents_get_api_endpoint();

    const char *scheme = ESP_AGENT_API_USE_TLS ? "wss" : "ws";
    size_t scheme_len = strlen(scheme) + 3; /*  for "://" */

    size_t buf_len = scheme_len + strlen(api_url) + strlen("/user/agents/") + strlen(agent_id) + strlen("/ws") + strlen("?token=") + strlen(access_token) + 1;
    char *buf = malloc(buf_len);
    if (buf == NULL) {
        return ESP_ERR_NO_MEM;
    }
    snprintf(buf, buf_len, "%s://%s/user/agents/%s/ws?token=%s", scheme, api_url, agent_id, access_token);
    *uri_out = buf;
    *uri_len = buf_len;
    return ESP_OK;
}

esp_err_t esp_agent_websocket_start(esp_agent_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_agent_t *agent = (esp_agent_t *)handle;

    esp_err_t ret = ESP_OK;
    size_t access_token_len = 0;
    char *ws_uri = NULL;
    size_t ws_uri_len = 0;
    const int64_t access_token_margin_us = (int64_t)(ACCESS_TOKEN_EXPIRATION_SECONDS - 10) * 1000000LL;
    /* Check if access token is expired or not set, leave 10 seconds margin */
    if (agent->access_token == NULL || (esp_timer_get_time() - agent->access_token_timestamp > access_token_margin_us)) {
        if (agent->access_token) {
            free(agent->access_token);
            agent->access_token = NULL;
        }
        ESP_GOTO_ON_ERROR(esp_agent_auth_get_access_token(agent->refresh_token, &agent->access_token, &access_token_len), end, TAG, "Failed to get access token");
        agent->access_token_timestamp = esp_timer_get_time();
        ESP_LOGD(TAG, "Access token: %s", agent->access_token);
    } else {
        ESP_LOGI(TAG, "Using existing access token, will expire in %lld seconds", (ACCESS_TOKEN_EXPIRATION_SECONDS - (esp_timer_get_time() - agent->access_token_timestamp) / 1000000));
    }

    ESP_GOTO_ON_ERROR(build_ws_uri(agent->agent_id, agent->access_token, &ws_uri, &ws_uri_len), end, TAG, "Failed to build websocket URI");
    ESP_LOGD(TAG, "Websocket URI: %s", ws_uri);

    esp_websocket_client_set_uri(agent->ws_client, ws_uri);

    ESP_LOGI(TAG, "Starting agent");

    ESP_GOTO_ON_ERROR(esp_websocket_client_start(agent->ws_client), end, TAG, "Failed to start websocket client");

end:
    if (ws_uri) {
        free(ws_uri);
    }
    return ret;
}

/* Start the agent connection */
esp_err_t esp_agent_start(esp_agent_handle_t handle, const char *conversation_id)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_agent_t *agent = (esp_agent_t *)handle;

    if (agent->started) {
        ESP_LOGW(TAG, "Agent already started");
        return ESP_OK;
    }

    // Check if agent_id is set
    if (agent->agent_id == NULL) {
        ESP_LOGE(TAG, "Agent ID not set. Use esp_agent_set_agent_id() to set it");
        return ESP_ERR_INVALID_STATE;
    }

    // Check if refresh_token is set
    if (agent->refresh_token == NULL) {
        ESP_LOGE(TAG, "Refresh token not set. Use esp_agent_set_refresh_token() to set it");
        return ESP_ERR_INVALID_STATE;
    }

    // Store conversation_id if provided
    if (conversation_id != NULL) {
        if (agent->conversation_id) {
            free(agent->conversation_id);
        }
        agent->conversation_id = strdup(conversation_id);
        if (agent->conversation_id == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for conversation_id");
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t err = esp_agent_websocket_start(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start websocket: %x", err);
        return err;
    }

    agent->started = true;
    return ESP_OK;
}

/* Stop the agent connection */
esp_err_t esp_agent_stop(esp_agent_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_agent_t *agent = (esp_agent_t *)handle;

    if (!agent->started) {
        ESP_LOGW(TAG, "Agent not started");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping agent");

    /* Stop websocket connection */
    if (agent->connected) {
        esp_websocket_client_close(agent->ws_client, pdMS_TO_TICKS(100));
        esp_websocket_client_stop(agent->ws_client);
    }

    agent->started = false;
    agent->connected = false;
    agent->handshake_state = ESP_AGENT_HANDSHAKE_NOT_DONE;

    // Purge any remaining messages in send queue
    if (agent->send_queue) {
        ws_send_message_t *msg = NULL;
        while (xQueueReceive(agent->send_queue, &msg, 0) == pdTRUE) {
            if (msg) {
                if (msg->payload) {
                    free(msg->payload);
                }
                free(msg);
            }
        }
    }

    return ESP_OK;
}

static esp_err_t send_handshake(esp_agent_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_agent_t *agent = (esp_agent_t *)handle;

    if (agent->handshake_state != ESP_AGENT_HANDSHAKE_NOT_DONE) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;

    char *handshake_json_str = esp_agent_messages_get_handshake(handle);
    ESP_GOTO_ON_FALSE(handshake_json_str, ESP_ERR_INVALID_STATE, end, TAG, "Failed to get handshake string");

    ESP_LOGD(TAG, "Sending Handshake: %s", handshake_json_str);

    ESP_LOGI(TAG, "Sending handshake for conversation mode: %s", agent->conversation_type == ESP_AGENT_CONVERSATION_SPEECH ? "audio" : "text");
    ret = esp_agent_websocket_queue_message(agent, WS_SEND_MSG_TYPE_TEXT, handshake_json_str, strlen(handshake_json_str), portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to queue handshake: %d", ret);
        goto end;
    }

    agent->handshake_state = ESP_AGENT_HANDSHAKE_AWAITING_ACK;

end:
    if (handshake_json_str) {
        free(handshake_json_str);
    }
    return ret;
}

/* Websocket event handler */
void esp_agent_websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "WebSocket event: %d", event_id);
    static char *message_buffer = NULL;
    static size_t message_buffer_size = 0;
    static size_t message_buffer_capacity = 0;

    esp_agent_t *agent = (esp_agent_t *)handler_args;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected");
            if (agent->handshake_state == ESP_AGENT_HANDSHAKE_NOT_DONE) {
                send_handshake(agent);
            }
            agent->connected = true;
            esp_agent_post_event(agent, ESP_AGENT_EVENT_CONNECTED, NULL);
            break;

        case WEBSOCKET_EVENT_DATA:
            if (data->op_code == WS_TRANSPORT_OPCODES_TEXT) {
                ESP_LOGD(TAG, "Received text chunk: %.*s", data->data_len, (char *)data->data_ptr);

                // Reallocate buffer if needed
                size_t new_size = message_buffer_size + data->data_len;
                if (new_size >= message_buffer_capacity) {
                    size_t new_capacity = message_buffer_capacity * 2;
                    if (new_capacity < new_size + 1) {
                        new_capacity = new_size + 1;
                    }

                    char *new_buffer = realloc(message_buffer, new_capacity);
                    if (new_buffer == NULL) {
                        ESP_LOGE(TAG, "Failed to reallocate message buffer");
                        break;
                    }
                    message_buffer = new_buffer;
                    message_buffer_capacity = new_capacity;
                }

                memcpy(message_buffer + message_buffer_size,
                       data->data_ptr, data->data_len);
                message_buffer_size += data->data_len;
                message_buffer[message_buffer_size] = '\0';

                // Check if we have a complete JSON message
                cJSON *test_json = cJSON_Parse(message_buffer);
                if (test_json != NULL) {
                    // Valid JSON found - process it
                    char *complete_message = strdup(message_buffer);
                    if (complete_message != NULL) {
                        int err = xQueueSend(agent->message_queue, &complete_message, pdMS_TO_TICKS(10));
                        if (err != pdTRUE) {
                            ESP_LOGE(TAG, "Failed to send complete message to queue");
                            free(complete_message);
                        }
                    }

                    message_buffer_size = 0;
                    message_buffer[0] = '\0';

                    cJSON_Delete(test_json);
                } else if (message_buffer_size > 64 * 1024) { // 64KB limit
                        // Check if buffer is getting too large (prevent memory issues)

                        /* FIXME: EDGE CASE: If there is beginning of a valid JSON mesage while this happens,
                         * None of the subsequent messages could be parsed until the websocket is reconnected.
                         * One solution might be to check the beginning of new message with by checking if it
                         * starts with `{"type"`
                         */
                        ESP_LOGW(TAG, "Incoming text message buffer too large, resetting");
                        message_buffer_size = 0;
                        message_buffer[0] = '\0';
                }

            } else if (data->op_code == WS_TRANSPORT_OPCODES_BINARY) {
                ESP_LOGV(TAG, "Received speech data: %d bytes", data->data_len);
                uint8_t *audio_buf = malloc(data->data_len);
                if (!audio_buf) {
                    ESP_LOGE(TAG, "Failed to allocate %d bytes for speech data", data->data_len);
                    break;
                }

                memcpy(audio_buf, (char *)data->data_ptr, data->data_len);
                esp_agent_message_data_t message_data = {
                    .speech = {
                        .data = audio_buf,
                        .len = data->data_len,
                    },
                };
                esp_agent_post_event(agent, ESP_AGENT_EVENT_DATA_TYPE_SPEECH, &message_data);
            }
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
        case WEBSOCKET_EVENT_ERROR:
        case WEBSOCKET_EVENT_CLOSED:
        case WEBSOCKET_EVENT_FINISH: /* This event is emitted when websocket task stops processing */
            ESP_LOGE(TAG, "WebSocket disconnected: %d", event_id);
            agent->connected = false;
            agent->started = false;
            /* Perform handshake again on reconnect */
            agent->handshake_state = ESP_AGENT_HANDSHAKE_NOT_DONE;
            esp_agent_post_event(agent, ESP_AGENT_EVENT_DISCONNECTED, NULL);

            // Reset message buffer on error
            if (message_buffer) {
                free(message_buffer);
                message_buffer = NULL;
                message_buffer_size = 0;
                message_buffer_capacity = 0;
            }
            break;

        default:
            break;
    }
}
