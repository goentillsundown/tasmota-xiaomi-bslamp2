# Tasmota for Xiaomi Mijia Bedside Lamp 2

Unofficial Tasmota port for the Xiaomi Mijia Bedside Lamp 2, model MJCTD02YL.

This project adds support for the lamp's:

- RGB + white LED output
- color temperature control
- brightness control
- front touch panel
- power button
- color preset button
- touch slider
- front-panel LED indicators
- Xiaomi-style night-light mode

The lamp uses an ESP32-S0WD-OEM with 4 MB flash.

## Status

The port is functional and has been tested on real hardware by real people. Is this real or fantasy.

Known limitation:

- Under extreme rapid button spamming, the front-panel I2C controller can still occasionally return an I2C error. Normal operation is unaffected.

## Hardware GPIO mapping

| Function | GPIO |
|---|---:|
| Red PWM | 13 |
| Green PWM | 14 |
| Blue PWM | 5 |
| White PWM | 12 |
| LED enable 1 | 33 |
| LED enable 2 | 4 |
| Front-panel IRQ | 16 |
| Front-panel I2C SCL | 19 |
| Front-panel I2C SDA | 21 |
| EEPROM SDA | 17 |
| EEPROM SCL | 18 |

## Source and attribution

This implementation is based on Tasmota and on the reverse-engineering work by Maurice Makaay for the ESPHome Xiaomi Bedside Lamp 2 project.

Tasmota:

https://github.com/arendst/Tasmota

ESPHome Xiaomi Bedside Lamp 2:

https://github.com/mmakaay/esphome-xiaomi_bslamp2

The Tasmota-specific driver, light integration, build configuration, and adaptation for the Xiaomi Mijia Bedside Lamp 2 are provided by this project.

## Tasmota base version

This build was developed and tested against Tasmota commit:

```text
68157a26297af6e20564b199cdddf47b4242dbfa

##Supported Hardware

Xiaomi Mijia Bedside Lamp 2
Model: MJCTD02YL
SoC: ESP32-S0WD-OEM
Flash: 4 MB
Flash mode: DIO
Flash frequency: 40 MHz
CPU: single-core ESP32

##Build target

tasmota32-xiaomi-bslamp2
tasmota32solo1

##Source Files

source/
├── platformio_override.ini
├── platformio_tasmota_cenv.ini
├── user_config_override.h
├── xdrv_124_xiaomi_bslamp2.ino
├── xiaomi_bslamp2.h
├── xiaomi_bslamp2_light.h
└── xlgt_12_xiaomi_bslamp2.ino
