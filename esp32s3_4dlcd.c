/*
 * 4D Systems Pty Ltd
 * www.4dsystems.com.au
 *
 * SPDX-FileCopyrightText: 
 *   - 2022-2023 Espressif Systems (Shanghai) CO LTD
 *   - 4D Systems Pty Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <sys/cdefs.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"

#include "esp32s3_4dlcd.h"

static const char *TAG = "esp32s3_4dlcd";

static esp_err_t esp32s3_4dlcd_del(esp_lcd_panel_t *panel);
static esp_err_t esp32s3_4dlcd_reset(esp_lcd_panel_t *panel);
static esp_err_t esp32s3_4dlcd_init(esp_lcd_panel_t *panel);
static esp_err_t esp32s3_4dlcd_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t esp32s3_4dlcd_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t esp32s3_4dlcd_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t esp32s3_4dlcd_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t esp32s3_4dlcd_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t esp32s3_4dlcd_disp_on_off(esp_lcd_panel_t *panel, bool off);

#if defined(CONFIG_LCD_INTERFACE_QSPI)
#define LCD_OPCODE_WRITE_CMD        (0x02ULL)
#define LCD_OPCODE_READ_CMD         (0x03ULL)
#define LCD_OPCODE_WRITE_COLOR      (0x32ULL)

static esp_err_t tx_param(esp_lcd_panel_io_handle_t io, int lcd_cmd, const void *param, size_t param_size)
{
    lcd_cmd &= 0xff;
    lcd_cmd <<= 8;
    lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
    return esp_lcd_panel_io_tx_param(io, lcd_cmd, param, param_size);
}

static esp_err_t tx_color(esp_lcd_panel_io_handle_t io, int lcd_cmd, const void *param, size_t param_size)
{
    lcd_cmd &= 0xff;
    lcd_cmd <<= 8;
    lcd_cmd |= LCD_OPCODE_WRITE_COLOR << 24;
    return esp_lcd_panel_io_tx_color(io, lcd_cmd, param, param_size);
}
#else // CONFIG_LCD_INTERFACE_SPI
#define tx_param(io, lcd_cmd, param, param_size) \
    esp_lcd_panel_io_tx_param(io, lcd_cmd, param, param_size)
#define tx_color(io, lcd_cmd, param, param_size) \
    esp_lcd_panel_io_tx_color(io, lcd_cmd, param, param_size)
#endif

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // save current value of LCD_CMD_COLMOD register
    const esp32s3_4dlcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
} esp32s3_4dlcd_panel_t;

esp_err_t esp_lcd_new_esp32s3_4dlcd(const esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    esp32s3_4dlcd_panel_t *esp32s3_4dlcd = NULL;
    gpio_config_t io_conf = { 0 };

    ESP_GOTO_ON_FALSE(io && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    esp32s3_4dlcd = (esp32s3_4dlcd_panel_t *)calloc(1, sizeof(esp32s3_4dlcd_panel_t));
    ESP_GOTO_ON_FALSE(esp32s3_4dlcd, ESP_ERR_NO_MEM, err, TAG, "no mem for esp32s3_4dlcd panel");

#if LCD_RST_GPIO_NUM >= 0
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 1ULL << LCD_RST_GPIO_NUM;
    ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
#endif

#if (LCD_COLOR_ORDER == LCD_RGB_ELEMENT_ORDER_RGB)
    esp32s3_4dlcd->madctl_val = 0;
#elif (LCD_COLOR_ORDER == LCD_RGB_ELEMENT_ORDER_BGR)
    esp32s3_4dlcd->madctl_val |= LCD_CMD_BGR_BIT;
#else
    ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported rgb endian");
#endif

#if (LCD_BITS_PER_PIXEL == 16)
    esp32s3_4dlcd->colmod_val = 0x55; // 16 bits per pixel, RGB565 format
    esp32s3_4dlcd->fb_bits_per_pixel = 16;
#elif (LCD_BITS_PER_PIXEL == 18)
    esp32s3_4dlcd->colmod_val = 0x66;
    // each color component (R/G/B) should occupy the 6 high bits of a byte, which means 3 full bytes are required for a pixel
    esp32s3_4dlcd->fb_bits_per_pixel = 24;
#else
    ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
#endif

    esp32s3_4dlcd->io = io;
    esp32s3_4dlcd->reset_gpio_num = LCD_RST_GPIO_NUM;
    esp32s3_4dlcd->reset_level = LCD_RST_ACTIVE_HIGH;
    esp32s3_4dlcd->base.del = esp32s3_4dlcd_del;
    esp32s3_4dlcd->base.reset = esp32s3_4dlcd_reset;
    esp32s3_4dlcd->base.init = esp32s3_4dlcd_init;
    esp32s3_4dlcd->base.draw_bitmap = esp32s3_4dlcd_draw_bitmap;
    esp32s3_4dlcd->base.invert_color = esp32s3_4dlcd_invert_color;
    esp32s3_4dlcd->base.set_gap = esp32s3_4dlcd_set_gap;
    esp32s3_4dlcd->base.mirror = esp32s3_4dlcd_mirror;
    esp32s3_4dlcd->base.swap_xy = esp32s3_4dlcd_swap_xy;
    esp32s3_4dlcd->base.disp_on_off = esp32s3_4dlcd_disp_on_off;

    *ret_panel = &(esp32s3_4dlcd->base);
    ESP_LOGD(TAG, "new esp32s3_4dlcd panel @%p", esp32s3_4dlcd);

    ESP_LOGI(TAG, "LCD panel create success");

    return ESP_OK;

err:
    if (esp32s3_4dlcd) {
#if LCD_RST_GPIO_NUM >= 0
        gpio_reset_pin(LCD_RST_GPIO_NUM);
#endif
        free(esp32s3_4dlcd);
    }
    return ret;
}

static esp_err_t esp32s3_4dlcd_del(esp_lcd_panel_t *panel)
{
    esp32s3_4dlcd_panel_t *esp32s3_4dlcd = __containerof(panel, esp32s3_4dlcd_panel_t, base);

    if (esp32s3_4dlcd->reset_gpio_num >= 0) {
        gpio_reset_pin(esp32s3_4dlcd->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del esp32s3_4dlcd panel @%p", esp32s3_4dlcd);
    free(esp32s3_4dlcd);
    return ESP_OK;
}

static esp_err_t esp32s3_4dlcd_reset(esp_lcd_panel_t *panel)
{
    esp32s3_4dlcd_panel_t *esp32s3_4dlcd = __containerof(panel, esp32s3_4dlcd_panel_t, base);
    esp_lcd_panel_io_handle_t io = esp32s3_4dlcd->io;

    // perform hardware reset
    if (esp32s3_4dlcd->reset_gpio_num >= 0) {
        gpio_set_level(esp32s3_4dlcd->reset_gpio_num, esp32s3_4dlcd->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(esp32s3_4dlcd->reset_gpio_num, !esp32s3_4dlcd->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
    } else { // perform software reset
        ESP_RETURN_ON_ERROR(tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(20)); // spec, wait at least 5ms before sending new command
    }

    return ESP_OK;
}

static const esp32s3_4dlcd_init_cmd_t vendor_specific_init_default[] = {
#if defined(CONFIG_ESP32S3_4DLCD_24) || \
    defined(CONFIG_ESP32S3_4DLCD_28) || \
    defined(CONFIG_ESP32S3_4DLCD_32)
    { 0x11, NULL, 0, 120 },
    { 0x13, NULL, 0, 0 },
    { 0xEF, (uint8_t[]) { 0x01, 0x01, 0x00 }, 3, 0 },
    { 0xCF, (uint8_t[]) { 0x00, 0xC1, 0x30 }, 3, 0 },
    { 0xED, (uint8_t[]) { 0x64, 0x03, 0x12, 0x81 }, 4, 0 },
    { 0xE8, (uint8_t[]) { 0x85, 0x00, 0x7a }, 3, 0 },
    { 0xCB, (uint8_t[]) { 0x39, 0x2C, 0x00, 0x34, 0x02 }, 5, 0 },
    { 0xF7, (uint8_t[]) { 0x20 }, 1, 0 },
    { 0xEA, (uint8_t[]) { 0x00, 0x00}, 2, 0 },
    { 0xC0, (uint8_t[]) { 0x26 }, 1, 0 },
    { 0xC1, (uint8_t[]) { 0x11 }, 1, 0 },
    { 0xC5, (uint8_t[]) { 0x39, 0x27}, 2, 0},
    { 0xC7, (uint8_t[]) { 0xa6 }, 1, 0 },
    { 0x36, (uint8_t[]) { 0x48 }, 1, 0 },
    { 0x3A, (uint8_t[]) { 0x55 }, 1, 0 },
    { 0xB1, (uint8_t[]) { 0x00, 0x1b}, 2, 0},
    { 0xB6, (uint8_t[]) { 0x08, 0x82, 0x27}, 3, 0},
    { 0xF2, (uint8_t[]) { 0x00 }, 1, 0 },
    { 0x26, (uint8_t[]) { 0x01 }, 1, 0 },
    { 0xE0, (uint8_t[]) { 0x0F, 0x2d, 0x0e, 0x08, 0x12, 0x0a, 0x3d, 0x95, 0x31, 0x04, 0x10, 0x09, 0x09, 0x0d, 0x00}, 0, 0 },
    { 0xE1, (uint8_t[]) { 0x00, 0x12, 0x17, 0x03, 0x0d, 0x05, 0x2c, 0x44, 0x41, 0x05, 0x0F, 0x0a, 0x30, 0x32, 0x0F}, 15, 120 },
#if defined(CONFIG_ESP32S3_4DLCD_32)
    { 0x20, NULL, 0, 0 },
#else
    { 0x21, NULL, 0, 0 },
#endif
    { 0x29, NULL, 0, 120 },
#elif defined(CONFIG_ESP32S3_4DLCD_35)
    {0xE0, (uint8_t []){0x00, 0x13, 0x18, 0x04, 0x0F, 0x06, 0x3A, 0x56, 0x4D, 0x03, 0x0A, 0x06, 0x30, 0x3E, 0x0F}, 15, 0},
    {0xE1, (uint8_t []){0x00, 0x13, 0x18, 0x01, 0x11, 0x06, 0x38, 0x34, 0x4D, 0x06, 0x0D, 0x0B, 0x31, 0x37, 0x0F}, 15, 0},
    {0xC0, (uint8_t []){0x18, 0x16}, 2, 0},
    {0xC1, (uint8_t []){0x45}, 1, 0},
    {0xC5, (uint8_t []){0x00, 0x63, 0x01}, 3, 0},
    {0x36, (uint8_t []){0x48}, 1, 0},
    {0x3A, (uint8_t []){0x66}, 1, 0},
    {0xB0, (uint8_t []){0x00}, 1, 0},
    {0xB1, (uint8_t []){0xB0}, 1, 0},
    {0xB4, (uint8_t []){0x02}, 1, 0},
    {0xB6, (uint8_t []){0x02, 0x02}, 2, 0},
    {0xE9, (uint8_t []){0x00}, 1, 0},
    {0xF7, (uint8_t []){0xA9, 0x51, 0x2C, 0x82}, 4, 120},
    {0x11, NULL, 0, 120},
    {0x29, NULL, 0, 120},
    {0x21, NULL, 0, 120},
#elif defined(CONFIG_ESP32S3_4DLCD_43Q)
    {0x38, NULL, 0, 0},
    {0xff, (uint8_t []) {0xa5}, 1, 0},
    {0xe7, (uint8_t []) {0x10}, 1, 0},
    {0x35, (uint8_t []) {0x00}, 1, 0},
    {0x36, (uint8_t []) {0xc0}, 1, 0},
    {0x3A, (uint8_t []) {0x01}, 1, 0},
    {0x40, (uint8_t []) {0x01}, 1, 0},
    {0x41, (uint8_t []) {0x01}, 1, 0},
    {0x44, (uint8_t []) {0x15}, 1, 0},
    {0x45, (uint8_t []) {0x15}, 1, 0},
    {0x7d, (uint8_t []) {0x02}, 1, 0},
    {0xc1, (uint8_t []) {0xbb}, 1, 0},
    {0xc2, (uint8_t []) {0x05}, 1, 0},
    {0xc3, (uint8_t []) {0x10}, 1, 0},
    {0xc6, (uint8_t []) {0x3e}, 1, 0},
    {0xc7, (uint8_t []) {0x25}, 1, 0},
    {0xc8, (uint8_t []) {0x11}, 1, 0},
    {0x7a, (uint8_t []) {0x5f}, 1, 0},
    {0x6f, (uint8_t []) {0x44}, 1, 0},
    {0x78, (uint8_t []) {0x70}, 1, 0},
    {0xc9, (uint8_t []) {0x00}, 1, 0},
    {0x67, (uint8_t []) {0x21}, 1, 0},
    {0x51, (uint8_t []) {0x0a}, 1, 0},
    {0x52, (uint8_t []) {0x76}, 1, 0},
    {0x53, (uint8_t []) {0x0a}, 1, 0},
    {0x54, (uint8_t []) {0x76}, 1, 0},
    {0x46, (uint8_t []) {0x0a}, 1, 0},
    {0x47, (uint8_t []) {0x2a}, 1, 0},
    {0x48, (uint8_t []) {0x0a}, 1, 0},
    {0x49, (uint8_t []) {0x1a}, 1, 0},
    {0x56, (uint8_t []) {0x43}, 1, 0},
    {0x57, (uint8_t []) {0x42}, 1, 0},
    {0x58, (uint8_t []) {0x3c}, 1, 0},
    {0x59, (uint8_t []) {0x64}, 1, 0},
    {0x5a, (uint8_t []) {0x41}, 1, 0},
    {0x5b, (uint8_t []) {0x3c}, 1, 0},
    {0x5c, (uint8_t []) {0x02}, 1, 0},
    {0x5d, (uint8_t []) {0x3c}, 1, 0},
    {0x5e, (uint8_t []) {0x1f}, 1, 0},
    {0x60, (uint8_t []) {0x80}, 1, 0},
    {0x61, (uint8_t []) {0x3f}, 1, 0},
    {0x62, (uint8_t []) {0x21}, 1, 0},
    {0x63, (uint8_t []) {0x07}, 1, 0},
    {0x64, (uint8_t []) {0xe0}, 1, 0},
    {0x65, (uint8_t []) {0x02}, 1, 0},
    {0xca, (uint8_t []) {0x20}, 1, 0},
    {0xcb, (uint8_t []) {0x52}, 1, 0},
    {0xcc, (uint8_t []) {0x10}, 1, 0},
    {0xcd, (uint8_t []) {0x42}, 1, 0},
    {0xd0, (uint8_t []) {0x20}, 1, 0},
    {0xd1, (uint8_t []) {0x52}, 1, 0},
    {0xd2, (uint8_t []) {0x10}, 1, 0},
    {0xd3, (uint8_t []) {0x42}, 1, 0},
    {0xd4, (uint8_t []) {0x0a}, 1, 0},
    {0xd5, (uint8_t []) {0x32}, 1, 0},
    ///test  mode
    {0xf8, (uint8_t []) {0x03}, 1, 0},
    {0xf9, (uint8_t []) {0x20}, 1, 0},
    {0x80, (uint8_t []) {0x00}, 1, 0},
    {0xa0, (uint8_t []) {0x00}, 1, 0},
    {0x81, (uint8_t []) {0x07}, 1, 0},
    {0xa1, (uint8_t []) {0x06}, 1, 0},
    {0x82, (uint8_t []) {0x02}, 1, 0},
    {0xa2, (uint8_t []) {0x01}, 1, 0},
    {0x86, (uint8_t []) {0x11}, 1, 0},
    {0xa6, (uint8_t []) {0x10}, 1, 0},
    {0x87, (uint8_t []) {0x27}, 1, 0},
    {0xa7, (uint8_t []) {0x27}, 1, 0},
    {0x83, (uint8_t []) {0x37}, 1, 0},
    {0xa3, (uint8_t []) {0x37}, 1, 0},
    {0x84, (uint8_t []) {0x35}, 1, 0},
    {0xa4, (uint8_t []) {0x35}, 1, 0},
    {0x85, (uint8_t []) {0x3f}, 1, 0},
    {0xa5, (uint8_t []) {0x3f}, 1, 0},
    {0x88, (uint8_t []) {0x0b}, 1, 0},
    {0xa8, (uint8_t []) {0x0b}, 1, 0},
    {0x89, (uint8_t []) {0x14}, 1, 0},
    {0xa9, (uint8_t []) {0x14}, 1, 0},
    {0x8a, (uint8_t []) {0x1a}, 1, 0},
    {0xaa, (uint8_t []) {0x1a}, 1, 0},
    {0x8b, (uint8_t []) {0x0a}, 1, 0},
    {0xab, (uint8_t []) {0x0a}, 1, 0},
    {0x8c, (uint8_t []) {0x14}, 1, 0},
    {0xac, (uint8_t []) {0x08}, 1, 0},
    {0x8d, (uint8_t []) {0x17}, 1, 0},
    {0xad, (uint8_t []) {0x07}, 1, 0},
    {0x8e, (uint8_t []) {0x16}, 1, 0},
    {0xae, (uint8_t []) {0x06}, 1, 0},
    {0x8f, (uint8_t []) {0x1b}, 1, 0},
    {0xaf, (uint8_t []) {0x07}, 1, 0},
    {0x90, (uint8_t []) {0x04}, 1, 0},
    {0xb0, (uint8_t []) {0x04}, 1, 0},
    {0x91, (uint8_t []) {0x0a}, 1, 0},
    {0xb1, (uint8_t []) {0x0a}, 1, 0},
    {0x92, (uint8_t []) {0x16}, 1, 0},
    {0xb2, (uint8_t []) {0x15}, 1, 0},
    {0xff, (uint8_t []) {0x00}, 1, 0},
    {0x11, (uint8_t []) {0x00}, 1, 700},
    {0x29, (uint8_t []) {0x00}, 1, 100},
#else
#error "No valid 4D Systems LCD model defined"
#endif
};

static esp_err_t esp32s3_4dlcd_init(esp_lcd_panel_t *panel)
{
    esp32s3_4dlcd_panel_t *esp32s3_4dlcd = __containerof(panel, esp32s3_4dlcd_panel_t, base);
    esp_lcd_panel_io_handle_t io = esp32s3_4dlcd->io;

    // LCD goes into sleep mode and display will be turned off after power on reset, exit sleep mode first
    ESP_RETURN_ON_ERROR(tx_param(io, LCD_CMD_SLPOUT, NULL, 0), TAG, "send command failed");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(tx_param(io, LCD_CMD_MADCTL, ((uint8_t[]) {
        esp32s3_4dlcd->madctl_val,
    }), 1), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(tx_param(io, LCD_CMD_COLMOD, ((uint8_t[]) {
        esp32s3_4dlcd->colmod_val,
    }), 1), TAG, "send command failed");

    const esp32s3_4dlcd_init_cmd_t *init_cmds = vendor_specific_init_default;
    uint16_t init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(esp32s3_4dlcd_init_cmd_t);

    bool is_cmd_overwritten = false;
    for (int i = 0; i < init_cmds_size; i++) {
        // Check if the command has been used or conflicts with the internal
        switch (init_cmds[i].cmd) {
        case LCD_CMD_MADCTL:
            is_cmd_overwritten = true;
            esp32s3_4dlcd->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        case LCD_CMD_COLMOD:
            is_cmd_overwritten = true;
            esp32s3_4dlcd->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        default:
            is_cmd_overwritten = false;
            break;
        }

        if (is_cmd_overwritten) {
            ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence", init_cmds[i].cmd);
        }

        // TODO: this might not work for QSPI 4.3" display
        ESP_RETURN_ON_ERROR(tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
    }
    ESP_LOGD(TAG, "send init commands success");

    return ESP_OK;
}

static esp_err_t esp32s3_4dlcd_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    esp32s3_4dlcd_panel_t *esp32s3_4dlcd = __containerof(panel, esp32s3_4dlcd_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = esp32s3_4dlcd->io;

    x_start += esp32s3_4dlcd->x_gap;
    x_end += esp32s3_4dlcd->x_gap;
    y_start += esp32s3_4dlcd->y_gap;
    y_end += esp32s3_4dlcd->y_gap;

    // define an area of frame memory where MCU can access
    ESP_RETURN_ON_ERROR(tx_param(io, LCD_CMD_CASET, ((uint8_t[]) {
        (x_start >> 8) & 0xFF,
        x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF,
        (x_end - 1) & 0xFF,
    }), 4), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(tx_param(io, LCD_CMD_RASET, ((uint8_t[]) {
        (y_start >> 8) & 0xFF,
        y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF,
        (y_end - 1) & 0xFF,
    }), 4), TAG, "send command failed");
    // transfer frame buffer
    size_t len = (x_end - x_start) * (y_end - y_start) * esp32s3_4dlcd->fb_bits_per_pixel / 8;
    tx_color(io, LCD_CMD_RAMWR, color_data, len);

    return ESP_OK;
}

static esp_err_t esp32s3_4dlcd_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    esp32s3_4dlcd_panel_t *esp32s3_4dlcd = __containerof(panel, esp32s3_4dlcd_panel_t, base);
    esp_lcd_panel_io_handle_t io = esp32s3_4dlcd->io;
    int command = 0;
    if (invert_color_data) {
        command = LCD_CMD_INVON;
    } else {
        command = LCD_CMD_INVOFF;
    }
    ESP_RETURN_ON_ERROR(tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t esp32s3_4dlcd_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    esp32s3_4dlcd_panel_t *esp32s3_4dlcd = __containerof(panel, esp32s3_4dlcd_panel_t, base);
    esp_lcd_panel_io_handle_t io = esp32s3_4dlcd->io;
    if (mirror_x) {
        esp32s3_4dlcd->madctl_val |= LCD_CMD_MX_BIT;
    } else {
        esp32s3_4dlcd->madctl_val &= ~LCD_CMD_MX_BIT;
    }
    if (mirror_y) {
        esp32s3_4dlcd->madctl_val |= LCD_CMD_MY_BIT;
    } else {
        esp32s3_4dlcd->madctl_val &= ~LCD_CMD_MY_BIT;
    }
    ESP_RETURN_ON_ERROR(tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        esp32s3_4dlcd->madctl_val
    }, 1), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t esp32s3_4dlcd_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    esp32s3_4dlcd_panel_t *esp32s3_4dlcd = __containerof(panel, esp32s3_4dlcd_panel_t, base);
    esp_lcd_panel_io_handle_t io = esp32s3_4dlcd->io;
    if (swap_axes) {
        esp32s3_4dlcd->madctl_val |= LCD_CMD_MV_BIT;
    } else {
        esp32s3_4dlcd->madctl_val &= ~LCD_CMD_MV_BIT;
    }
    ESP_RETURN_ON_ERROR(tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        esp32s3_4dlcd->madctl_val
    }, 1), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t esp32s3_4dlcd_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    esp32s3_4dlcd_panel_t *esp32s3_4dlcd = __containerof(panel, esp32s3_4dlcd_panel_t, base);
    esp32s3_4dlcd->x_gap = x_gap;
    esp32s3_4dlcd->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t esp32s3_4dlcd_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    esp32s3_4dlcd_panel_t *esp32s3_4dlcd = __containerof(panel, esp32s3_4dlcd_panel_t, base);
    esp_lcd_panel_io_handle_t io = esp32s3_4dlcd->io;
    int command = 0;
    if (on_off) {
        command = LCD_CMD_DISPON;
    } else {
        command = LCD_CMD_DISPOFF;
    }
    ESP_RETURN_ON_ERROR(tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}

esp_err_t backlight_init(void)
{
    // 1. Configure timer with 10-bit resolution
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LCD_BL_PWM_RESOLUTION,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = LCD_BL_PWM_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&ledc_timer), TAG, "backlight timer configuration failed");

    // 2. Configure channel
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LCD_BL_GPIO_NUM,
        .duty           = 0,    // Start with 0% duty cycle
        .hpoint         = 0
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ledc_channel), TAG, "backlight channel configuration failed");
    return ESP_OK;
}

// Set brightness (0-255)
esp_err_t backlight_set(uint8_t brightness)
{
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, brightness), TAG, "set backlight duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0), TAG, "update backlight duty failed");
    return ESP_OK;
}
