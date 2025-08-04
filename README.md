# 4D Systems' ESP32-S3 series displays

Implementation of the LCD controller with esp_lcd component for 4D Systems' ESP32-S3 series displays.

| 4DLCD Series                                                                                  | Supported | LCD Controller                                      | Interface | Link to LCD Datasheet |
|:--------------------------------------------------------------------------------------------- |:---------:|:---------------------------------------------------:|:---------:|:--------------------- |
| [gen4-ESP32-24 Series](https://resources.4dsystems.com.au/datasheets/esp32/gen4-esp32/)       | ✅       | [ILI9341](https://4dsystems.com.au/download/31395/) | SPI       | [Datasheet](https://resources.4dsystems.com.au/datasheets/4dlcd/4DLCD-24320240-IPS/) |
| [gen4-ESP32-28 Series](https://resources.4dsystems.com.au/datasheets/esp32/gen4-esp32/)       | ✅       | [ILI9341](https://4dsystems.com.au/download/31395/) | SPI       | [Datasheet](https://resources.4dsystems.com.au/datasheets/4dlcd/4DLCD-28320240-IPS/) |
| [gen4-ESP32-32 Series](https://resources.4dsystems.com.au/datasheets/esp32/gen4-esp32/)       | ✅       | [ILI9341](https://4dsystems.com.au/download/31395/) | SPI       | [Datasheet](https://resources.4dsystems.com.au/datasheets/4dlcd/4DLCD-32320240/)     |
| [gen4-ESP32-35 Series](https://resources.4dsystems.com.au/datasheets/esp32/gen4-esp32/)       | ✅       | [ILI9488](https://4dsystems.com.au/download/31399/) | SPI       | [Datasheet](https://resources.4dsystems.com.au/datasheets/4dlcd/4DLCD-35480320-IPS/) |
| [gen4-ESP32Q-43 Series](https://resources.4dsystems.com.au/datasheets/esp32/gen4-esp32Q-43/)  | ❌       | [NV3041A](https://4dsystems.com.au/download/31400/) | QSPI      | Not available         |

## Add to project

At the time of writing, this package is not yet publish in ESP-IDF Component Registry.

You can install this package by following the instructions for defining a dependency from Git repository found [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html#defining-dependencies-in-the-manifest).

Example:

``` yml
dependencies:
  # Define a dependency from a Git repository
  esp32s3_4dlcd:
    git: https://github.com/4dsystems/esp32s3_4dlcd.git
```