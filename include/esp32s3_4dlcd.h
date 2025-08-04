/*
 * 4D Systems Pty Ltd
 * www.4dsystems.com.au
 *
 * SPDX-FileCopyrightText: 
 *   - 2022-2023 Espressif Systems (Shanghai) CO LTD
 *   - 4D Systems Pty Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file
 * @brief ESP LCD: 4D Systems' ESP32-S3 Series
 */

#pragma once

#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_io_spi.h"
#include "esp_check.h"
#include "driver/ledc.h"

#include "4dlcd_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LCD panel initialization commands.
 *
 */
typedef struct {
    int cmd;                /*<! The specific LCD command */
    const void *data;       /*<! Buffer that holds the command specific data */
    size_t data_bytes;      /*<! Size of `data` in memory, in bytes */
    unsigned int delay_ms;  /*<! Delay in milliseconds after this command */
} esp32s3_4dlcd_init_cmd_t;

/**
 * @brief LCD panel vendor configuration.
 *
 * @note  This structure needs to be passed to the `vendor_config` field in `esp_lcd_panel_dev_config_t`.
 *
 */
typedef struct {
    const esp32s3_4dlcd_init_cmd_t *init_cmds;      /*!< Pointer to initialization commands array. Set to NULL if using default commands.
                                                         *   The array should be declared as `static const` and positioned outside the function.
                                                         *   Please refer to `vendor_specific_init_default` in source file.
                                                         */
    uint16_t init_cmds_size;                            /*<! Number of commands in above array */
} esp32s3_4dlcd_vendor_config_t;

/**
 * @brief Create LCD panel for 4D Systems ESP32-S3 series of displays
 *
 * @note  Vendor specific initialization can be different between manufacturers, should consult the LCD supplier for initialization sequence code.
 *
 * @param[in] io LCD panel IO handle
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *          - ESP_ERR_INVALID_ARG   if parameter is invalid
 *          - ESP_ERR_NO_MEM        if out of memory
 *          - ESP_OK                on success
 */
esp_err_t esp_lcd_new_esp32s3_4dlcd(const esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief LCD panel bus configuration structure
 *
 * @param[in] max_trans_sz Maximum transfer size in bytes
 *
 */
#if (LCD_SPI_IS_QUAD != 1)
#define ESP32S3_4DLCD_BUS_SPI_CONFIG(max_trans_sz)              \
    {                                                           \
        .sclk_io_num = LCD_SPI_SCLK_GPIO_NUM,                   \
        .mosi_io_num = LCD_SPI_MOSI_GPIO_NUM,                   \
        .miso_io_num = LCD_SPI_MISO_GPIO_NUM,                   \
        .quadhd_io_num = -1,                                    \
        .quadwp_io_num = -1,                                    \
        .max_transfer_sz = max_trans_sz,                        \
    }
#else /* LCD_SPI_IS_QUAD == 1 */
#define ESP32S3_4DLCD_BUS_SPI_CONFIG(max_trans_sz)              \
    {                                                           \
        .sclk_io_num = LCD_SPI_SCLK_GPIO_NUM,                   \
        .data0_io_num = LCD_QSPI_DAT0_GPIO_NUM,                 \
        .data1_io_num = LCD_QSPI_DAT1_GPIO_NUM,                 \
        .data2_io_num = LCD_QSPI_DAT2_GPIO_NUM,                 \
        .data3_io_num = LCD_QSPI_DAT3_GPIO_NUM,                 \
        .quadhd_io_num = -1,                                    \
        .quadwp_io_num = -1,                                    \
        .max_transfer_sz = max_trans_sz,                        \
        .flags = SPICOMMON_BUSFLAG_QUAD,                        \
    }
#endif /* LCD_SPI_IS_QUAD != 1 */

/**
 * @brief LCD panel IO configuration structure
 *
 * @param[in] cb Callback function when SPI transfer is done
 * @param[in] cb_ctx Callback function context
 *
 */
#define ESP32S3_4DLCD_IO_SPI_CONFIG(callback, callback_ctx)     \
    {                                                           \
        .cs_gpio_num = LCD_SPI_CS_GPIO_NUM,                     \
        .dc_gpio_num = LCD_SPI_DC_GPIO_NUM,                     \
        .spi_mode = 0,                                          \
        .pclk_hz = LCD_SPI_PCLK_MHZ * 1000 * 1000,              \
        .trans_queue_depth = 7,                                 \
        .on_color_trans_done = callback,                        \
        .user_ctx = callback_ctx,                               \
        .lcd_cmd_bits = 8,                                      \
        .lcd_param_bits = 8,                                    \
    }

esp_err_t backlight_init(void);
esp_err_t backlight_set(uint16_t brightness);

#ifdef __cplusplus
}
#endif
