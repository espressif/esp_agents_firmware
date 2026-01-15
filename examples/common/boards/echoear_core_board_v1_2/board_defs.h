/**
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* The name of a GPIO Device that will be used to indicate the device listening status
 * When device is listening, the LED will be turned on.
 * This if optional, will be ignored if not defined.
 */
#define INDICATOR_DEVICE_NAME "led_green"

/* Whether the board supports capacitive touch, and it's associated GPIO pin */
#define CAPACITIVE_TOUCH_SUPPORTED 1
#define CAPACITIVE_TOUCH_CHANNEL_GPIO 7

/* Whether the board supports LEDC backlight */
#define LEDC_BACKLIGHT_SUPPORTED 1

/* The URL of the guide for this board */
#define BOARD_DEVICE_MANUAL_URL "https://raw.githubusercontent.com/espressif/esp_agents_firmware/refs/heads/main/examples/common/boards/echoear_core_board_v1_2/README.md"
