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
#include <esp_crt_bundle.h>

#include <esp_agent.h>
#include <esp_agent_internal.h>
#include <esp_agent_auth.h>
#include <esp_agent_internal_messages.h>
#include <esp_agent_websocket.h>
#include <esp_agent_internal_tools.h>
#include <esp_agent_internal_events.h>

static const char *TAG = "esp_agent";

ESP_EVENT_DEFINE_BASE(AGENT_EVENT);

#define ESP_AGENT_TEXT_MESSAGE_QUEUE_SIZE 10
#define ESP_AGENT_SEND_QUEUE_SIZE 35

#define MESSAGE_TASK_EXIT_WAIT_MS 6000
#define SEND_TASK_EXIT_WAIT_MS 2000

static void message_processing_task(void *pvParameters)
{
    esp_agent_t *agent = (esp_agent_t *)pvParameters;
    char *message = NULL;

    ESP_LOGD(TAG, "Message Parsing Task Started");

    while (1) {
        /* Check for stop event first (non-blocking) */
        EventBits_t bits = xEventGroupGetBits(agent->event_group);
        if (bits & MESSAGE_TASK_STOP_BIT) {
            ESP_LOGD(TAG, "Message Parsing Task received stop signal, exiting");
            break;
        }

        if (xQueueReceive(agent->message_queue, &message, pdMS_TO_TICKS(100)) == pdTRUE) {
            esp_agent_messages_parse_process(agent, message);
            free(message);
        }
    }

    ESP_LOGD(TAG, "Message Parsing Task exiting cleanly");
    vTaskDelete(NULL);
}

esp_agent_handle_t esp_agent_init(const esp_agent_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid config");
        return NULL;
    }

    esp_agent_t *agent = calloc(1, sizeof(esp_agent_t));

    if (agent == NULL) {
        ESP_LOGE(TAG, "Failed to allocate agent");
        goto err;
    }

    agent->conversation_id = NULL;
    agent->conversation_type = config->conversation_type;

    if (config->agent_id != NULL) {
        agent->agent_id = strdup(config->agent_id);
        if (agent->agent_id == NULL) {
            ESP_LOGE(TAG, "Failed to allocate agent_id");
            goto err;
        }
    }

    if (config->refresh_token != NULL) {
        agent->refresh_token = strdup(config->refresh_token);
        if (agent->refresh_token == NULL) {
            ESP_LOGE(TAG, "Failed to allocate refresh_token");
            goto err;
        }
    }

    if (config->conversation_type == ESP_AGENT_CONVERSATION_SPEECH && (config->upload_audio_config == NULL || config->download_audio_config == NULL)) {
        ESP_LOGE(TAG, "Audio configuration is required for speech conversation");
        goto err;
    }

    agent->upload_audio_config = *config->upload_audio_config;
    agent->download_audio_config = *config->download_audio_config;

    // Configure websocket client
    esp_websocket_client_config_t ws_cfg = {
        .buffer_size = 8*1024,
        .network_timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_auto_reconnect = true,
    };

    esp_err_t err;

    esp_event_loop_args_t loop_args = {
        .queue_size = 10,
        .task_name = "agent_events",
        .task_priority = 5,
        .task_stack_size = 4096,
        .task_core_id = 0,
    };

    err = esp_event_loop_create(&loop_args, &agent->event_loop);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create event loop");
        goto err;
    }

    agent->message_queue = xQueueCreate(ESP_AGENT_TEXT_MESSAGE_QUEUE_SIZE, sizeof(char *));
    if (agent->message_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create message queue");
        goto err;
    }

    agent->send_queue = xQueueCreate(ESP_AGENT_SEND_QUEUE_SIZE, sizeof(ws_send_message_t *));
    if (agent->send_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create send queue");
        goto err;
    }

    agent->ws_client = esp_websocket_client_init(&ws_cfg);

    if (agent->ws_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize websocket client");
        goto err;
    }

    esp_websocket_register_events(agent->ws_client, WEBSOCKET_EVENT_ANY, esp_agent_websocket_event_handler, agent);
    esp_event_handler_instance_register_with(agent->event_loop, AGENT_EVENT, ESP_EVENT_ANY_ID, esp_agent_internal_event_handler, NULL, &agent->internal_event_handler);

    agent->connected = false;
    agent->started = false;
    agent->handshake_state = ESP_AGENT_HANDSHAKE_NOT_DONE;

    // Initialize local tools list
    agent->local_tools = NULL;

    // Create event group for task stop signals
    agent->event_group = xEventGroupCreate();
    if (agent->event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create task event group");
        goto err;
    }

    // Create message processing task
    xTaskCreate(
        message_processing_task,
        "agent_msg_task",
        4096,
        agent,
        5,
        &agent->message_task_handle
    );

    // Create websocket send task
    xTaskCreate(
        esp_agent_websocket_send_task,
        "agent_ws_send",
        4096,
        agent,
        5,
        &agent->send_task_handle
    );

    ESP_LOGI(TAG, "Agent initialized");

    return (esp_agent_handle_t)agent;

err:
    esp_agent_deinit((esp_agent_handle_t)agent);
    return NULL;
}

static void stop_task_gracefully(TaskHandle_t *task_handle, EventGroupHandle_t event_group,
                                  EventBits_t stop_bit, uint32_t timeout_ms, const char *task_name)
{
    if (!*task_handle) {
        return;
    }

    ESP_LOGD(TAG, "Signaling %s to stop", task_name);
    if (event_group) {
        xEventGroupSetBits(event_group, stop_bit);
        TickType_t start_time = xTaskGetTickCount();
        while (eTaskGetState(*task_handle) != eDeleted &&
               (xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(timeout_ms)) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    if (eTaskGetState(*task_handle) != eDeleted) {
        ESP_LOGW(TAG, "%s did not exit cleanly within timeout, forcefully deleting", task_name);
        vTaskDelete(*task_handle);
    }
    *task_handle = NULL;
}

/* Deinitialize the agent */
void esp_agent_deinit(esp_agent_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    esp_agent_t *agent = (esp_agent_t *)handle;

    if (agent->started) {
        esp_agent_stop(handle);
    }

    stop_task_gracefully(&agent->message_task_handle, agent->event_group,
                         MESSAGE_TASK_STOP_BIT, MESSAGE_TASK_EXIT_WAIT_MS, "Message task");
    stop_task_gracefully(&agent->send_task_handle, agent->event_group,
                         SEND_TASK_STOP_BIT, SEND_TASK_EXIT_WAIT_MS, "Send task");

    if (agent->event_group) {
        vEventGroupDelete(agent->event_group);
        agent->event_group = NULL;
    }

    if (agent->ws_client) {
        esp_websocket_client_destroy(agent->ws_client);
    }

    if (agent->message_queue) {
        /* Purge any remaining messages in received messages queue */
        char *message = NULL;
        while (xQueueReceive(agent->message_queue, &message, 0) == pdTRUE) {
            if (message) {
                free(message);
            }
        }
        vQueueDelete(agent->message_queue);
    }

    if (agent->send_queue) {
        /* Purge any remaining messages in send queue */
        ws_send_message_t *msg = NULL;
        while (xQueueReceive(agent->send_queue, &msg, 0) == pdTRUE) {
            if (msg) {
                if (msg->payload) {
                    free(msg->payload);
                }
                free(msg);
            }
        }
        vQueueDelete(agent->send_queue);
    }

    if (agent->agent_id) {
        free(agent->agent_id);
    }

    if (agent->conversation_id) {
        free(agent->conversation_id);
    }

    if (agent->refresh_token) {
        free((void *)agent->refresh_token);
    }

    if (agent->access_token) {
        free(agent->access_token);
    }

    // Clean up all registered local tools
    local_tool_node_t *tool_node = agent->local_tools;
    while (tool_node != NULL) {
        local_tool_node_t *next_node = tool_node->next;
        if (tool_node->name) {
            free(tool_node->name);
        }
        free(tool_node);
        tool_node = next_node;
    }

    free(agent);

    ESP_LOGI(TAG, "Agent deinitialized");
}

esp_err_t esp_agent_set_agent_id(esp_agent_handle_t handle, const char *agent_id)
{
    if (handle == NULL || agent_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_agent_t *agent = (esp_agent_t *)handle;

    bool agent_id_changed = (agent->agent_id == NULL || strncmp(agent->agent_id, agent_id, 32) != 0);

    // Free existing agent_id and set new one
    if (agent->agent_id) {
        free(agent->agent_id);
    }
    agent->agent_id = strdup(agent_id);
    if (agent->agent_id == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for agent_id");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Set agent_id to: %s", agent_id);

    // If agent was started and agent_id changed, reconnect with new agent_id
    if (agent_id_changed && agent->started) {
        esp_err_t err = esp_agent_stop(handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to stop agent during agent_id update: %x", err);
            return err;
        }

        err = esp_agent_start(handle, NULL);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to restart agent after agent_id update: %x", err);
            return err;
        }
    }

    return ESP_OK;
}

esp_err_t esp_agent_set_refresh_token(esp_agent_handle_t handle, const char *refresh_token)
{
    if (handle == NULL || refresh_token == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_agent_t *agent = (esp_agent_t *)handle;

    if (agent->refresh_token) {
        free((void *)agent->refresh_token);
    }
    agent->refresh_token = strdup(refresh_token);
    if (agent->refresh_token == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for refresh_token");
        return ESP_ERR_NO_MEM;
    }

    if (agent->access_token) {
        free(agent->access_token);
        agent->access_token = NULL;
    }

    /* If agent was started/connected, stop and restart it with new refresh token */
    if (agent->started) {
        esp_err_t err = esp_agent_stop(handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to stop agent during refresh_token update: %x", err);
            return err;
        }
        err = esp_agent_start(handle, NULL);
    }

    ESP_LOGI(TAG, "Set refresh_token");
    return ESP_OK;
}

char *esp_agents_get_api_endpoint(void)
{
    if (!ESP_AGENT_API_ENDPOINT) {
        return NULL;
    }

    char *schema_end_index = strstr(ESP_AGENT_API_ENDPOINT, "://");

    if (schema_end_index) {
        return schema_end_index + 3; // +3 for "://"
    }

    return ESP_AGENT_API_ENDPOINT;
}
