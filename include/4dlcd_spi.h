#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#define LCD_COLOR_ORDER         LCD_RGB_ELEMENT_ORDER_RGB // Default color order for 4D Systems displays
#define LCD_RST_ACTIVE_HIGH     0 // Reset pin active low

// Resolution and bits per pixel for different 4D Systems ESP32-S3 LCD models
#if defined(CONFIG_ESP32S3_4DLCD_35)
#define LCD_WIDTH               320
#define LCD_HEIGHT              480
#define LCD_BITS_PER_PIXEL      18
#elif defined(CONFIG_ESP32S3_4DLCD_43Q)
#define LCD_WIDTH               480
#define LCD_HEIGHT              272
#define LCD_BITS_PER_PIXEL      16
#else
#define LCD_WIDTH               240
#define LCD_HEIGHT              320
#define LCD_BITS_PER_PIXEL      16
#endif

// Pin definitions for SPI/QSPI, backlight, and reset
#if defined(CONFIG_ESP32S3_4DLCD_43Q)
#define LCD_SPI_IS_QUAD         1       // Use Quad SPI for 4.3" QSPI display
#define LCD_BL_GPIO_NUM         2       // GPIO for backlight control
#define LCD_RST_GPIO_NUM        8       // GPIO for LCD reset
#define LCD_SPI_CS_GPIO_NUM     6       // GPIO for SPI CS
#define LCD_SPI_DC_GPIO_NUM     -1      // GPIO for SPI DC (Data/Command) (not used)
#define LCD_SPI_SCLK_GPIO_NUM   5       // GPIO for SPI SCLK
#define LCD_QSPI_DAT0_GPIO_NUM  9       // GPIO for QSPI DATA0
#define LCD_QSPI_DAT1_GPIO_NUM  7       // GPIO for QSPI DATA1
#define LCD_QSPI_DAT2_GPIO_NUM  4       // GPIO for QSPI DATA2
#define LCD_QSPI_DAT3_GPIO_NUM  3       // GPIO for QSPI DATA3
#else
#define LCD_SPI_IS_QUAD         0       // Use SPI for other models
#define LCD_BL_GPIO_NUM         4       // GPIO for backlight control
#define LCD_RST_GPIO_NUM        7       // GPIO for LCD reset
#define LCD_SPI_CS_GPIO_NUM     -1      // GPIO for SPI CS (not used)
#define LCD_SPI_DC_GPIO_NUM     21      // GPIO for SPI DC (Data/Command)
#define LCD_SPI_SCLK_GPIO_NUM   14      // GPIO for SPI SCLK
#define LCD_SPI_MISO_GPIO_NUM   12      // GPIO for SPI MISO
#define LCD_SPI_MOSI_GPIO_NUM   13      // GPIO for SPI MOSI
#endif

#define LCD_SPI_PCLK_MHZ        60      // SPI clock frequency in MHz
#define LCD_BL_PWM_FREQ_HZ      1000    // PWM frequency (1kHz typical)
#define LCD_BL_PWM_RESOLUTION   LEDC_TIMER_10_BIT  // 10-bit resolution (0-1023)

#if defined(__cplusplus)
}
#endif