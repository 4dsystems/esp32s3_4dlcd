#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#define LCD_COLOR_ORDER         LCD_RGB_ELEMENT_ORDER_RGB // Default color order for 4D Systems displays
#define LCD_RST_ACTIVE_HIGH     0 // Reset pin active low

// Resolution and bits per pixel for different 4D Systems ESP32-S3 RGB LCD models
#define LCD_WIDTH               800
#define LCD_HEIGHT              480
#define LCD_BITS_PER_PIXEL      16

// Pin definitions for SPI/QSPI, backlight, and reset
// TODO

#define LCD_BL_PWM_FREQ_HZ      25000    // PWM frequency (25kHz)
#define LCD_BL_PWM_RESOLUTION   LEDC_TIMER_8_BIT  // 8-bit resolution (0-255)

#if defined(__cplusplus)
}
#endif