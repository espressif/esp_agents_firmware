/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_agent.h>
#include <esp_websocket_client.h>
#include <esp_timer.h>
#include <freertos/event_groups.h>

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(AGENT_EVENT);

#define ESP_AGENT_API_USE_TLS 1

/* Event group bits for task stop signals */
#define MESSAGE_TASK_STOP_BIT BIT0
#define SEND_TASK_STOP_BIT    BIT1

typedef enum {
    ESP_AGENT_HANDSHAKE_NOT_DONE,
    ESP_AGENT_HANDSHAKE_AWAITING_ACK,
    ESP_AGENT_HANDSHAKE_DONE,
} esp_agent_handshake_state_t;

/* Local tool node structure for simple linked list */
typedef struct local_tool_node {
    char *name;                                    /* Tool name (dynamically allocated) */
    esp_agent_tool_handler_t tool_handler;         /* Function pointer */
    void *user_data;                               /* User-provided context */
    struct local_tool_node *next;                 /* Next node in the list */
} local_tool_node_t;

/* Agent handle structure */
typedef struct {
    bool started;
    bool connected;
    int64_t access_token_timestamp;
    char *access_token;
    char *agent_id;
    char *conversation_id;
    const char *refresh_token;
    esp_agent_audio_config_t upload_audio_config;
    esp_agent_audio_config_t download_audio_config;
    esp_agent_conversation_type_t conversation_type;
    esp_event_handler_instance_t internal_event_handler;
    esp_agent_handshake_state_t handshake_state;
    esp_event_loop_handle_t event_loop;
    esp_websocket_client_handle_t ws_client;
    QueueHandle_t message_queue;
    TaskHandle_t message_task_handle;
    QueueHandle_t send_queue;
    TaskHandle_t send_task_handle;
    EventGroupHandle_t event_group;               /* Event group for task stop signals */
    local_tool_node_t *local_tools;               /* Head of linked list of registered local tools */
} esp_agent_t;

/* This function will strip the https:// prefix from the menuconfig URL */
char *esp_agents_get_api_endpoint(void);

#ifdef __cplusplus
}
#endif
