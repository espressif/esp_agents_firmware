/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_log.h>
#include <esp_check.h>

#include <esp_vfs.h>
#include <driver/usb_serial_jtag.h>
#include <driver/uart.h>

#include <esp_rmaker_common_console.h>

#include <agent_console.h>

static const char *TAG = "agent_console";

#define CMD_BUFFER_SIZE 3072

static esp_console_repl_t *g_repl = NULL;

void start_console_task(void *arg)
{
    esp_console_start_repl(g_repl);
    vTaskDelete(NULL);
}

void console_reconfigure_peripheral(void)
{
    /* Drain stdout before reconfiguring it */
    fflush(stdout);
    fsync(fileno(stdout));

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    const uart_config_t uart_config = {
            .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
#if SOC_UART_SUPPORT_REF_TICK
            .source_clk = UART_SCLK_REF_TICK,
#elif SOC_UART_SUPPORT_XTAL_CLK
            .source_clk = UART_SCLK_XTAL,
#endif
    };

    ESP_ERROR_CHECK( uart_driver_delete(CONFIG_ESP_CONSOLE_UART_NUM) );
    ESP_ERROR_CHECK( uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, CMD_BUFFER_SIZE, 0, 0, NULL, 0) );
    ESP_ERROR_CHECK( uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config) );

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    usb_serial_jtag_driver_config_t jtag_config = {
        .tx_buffer_size = 256,
        .rx_buffer_size = CMD_BUFFER_SIZE,
    };

    ESP_ERROR_CHECK( usb_serial_jtag_driver_uninstall() );
    ESP_ERROR_CHECK( usb_serial_jtag_driver_install(&jtag_config));

#endif
}

esp_err_t agent_console_init(void)
{
    if (g_repl) {
        ESP_LOGE(TAG, "Console REPL already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.max_cmdline_length = 3072; // 2KB required for refresh token, in worst case

    /* Note: console is not fully tested on UART */
#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_console_new_repl_uart(&hw_config, &repl_config, &repl), TAG, "Failed to create console REPL");
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &g_repl), TAG, "Failed to create console REPL");
#endif

    /* By default esp_console configures very short buffer rx size(256 bytes) for peripheral.
     * This leads to data being corrupted when receiving long strings (e.g. refresh token).
     * So we de-intialize and re-initialize the peripheral here.
     */
    console_reconfigure_peripheral();

    /* Start esp console from dedicated task so main task is not blocked when console couldn't be started
     * (Power without USB)
     */
    xTaskCreate(start_console_task, "start_console_task", 4096, NULL, 1, NULL);

    /* Register default commands */
    ESP_RETURN_ON_ERROR(agent_console_register_default_commands(), TAG, "Failed to register default commands");

    return ESP_OK;
}

esp_err_t agent_console_register_default_commands(void)
{
    if (!g_repl) {
        ESP_LOGE(TAG, "Console REPL not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_rmaker_common_register_commands();
    return ESP_OK;
}

esp_err_t agent_console_register_command(const esp_console_cmd_t *cmd)
{
    if (!g_repl) {
        ESP_LOGE(TAG, "Console REPL not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    return esp_console_cmd_register(cmd);
}
