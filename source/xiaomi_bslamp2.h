#pragma once


/*********************************************************************************************\
 * Xiaomi Mijia Bedside Lamp 2 (MJCTD02YL)
 *
 * Hardware platform:
 *
 *   SoC   : ESP32-S0WD-OEM
 *   Flash : 4 MB
 *
 * GPIO mapping based on the reverse-engineered Xiaomi Bedside Lamp 2
 * implementation by Maurice Makaay.
 *
 * LED outputs:
 *
 *   GPIO13 -> Red PWM
 *   GPIO14 -> Green PWM
 *   GPIO5  -> Blue PWM
 *   GPIO12 -> White PWM
 *
 * LED power-stage enables:
 *
 *   GPIO33 -> LED enable 1
 *   GPIO4  -> LED enable 2
 *
 * Front panel:
 *
 *   GPIO16 -> Interrupt
 *   GPIO19 -> I2C SCL
 *   GPIO21 -> I2C SDA
 *
 * EEPROM:
 *
 *   GPIO17 -> I2C SDA
 *   GPIO18 -> I2C SCL
\*********************************************************************************************/


/*********************************************************************************************\
 * LED PWM outputs
\*********************************************************************************************/

#define XIAOMI_BSLAMP2_GPIO_RED          13
#define XIAOMI_BSLAMP2_GPIO_GREEN        14
#define XIAOMI_BSLAMP2_GPIO_BLUE          5
#define XIAOMI_BSLAMP2_GPIO_WHITE        12


/*********************************************************************************************\
 * LED power-stage enables
 *
 * Both outputs must be HIGH for the lamp LEDs to operate.
\*********************************************************************************************/

#define XIAOMI_BSLAMP2_GPIO_LED_ENABLE_1 33
#define XIAOMI_BSLAMP2_GPIO_LED_ENABLE_2  4


/*********************************************************************************************\
 * Front-panel interface
\*********************************************************************************************/

#define XIAOMI_BSLAMP2_GPIO_PANEL_IRQ    16
#define XIAOMI_BSLAMP2_GPIO_PANEL_SCL    19
#define XIAOMI_BSLAMP2_GPIO_PANEL_SDA    21


/*********************************************************************************************\
 * EEPROM interface
\*********************************************************************************************/

#define XIAOMI_BSLAMP2_GPIO_EEPROM_SDA   17
#define XIAOMI_BSLAMP2_GPIO_EEPROM_SCL   18


/*********************************************************************************************\
 * Xiaomi light conversion implementation
 *
 * This header can also be included indirectly while C sources are being
 * compiled by Tasmota, so the C++-only light conversion code must be guarded.
\*********************************************************************************************/

#ifdef __cplusplus
#include "xiaomi_bslamp2_light.h"
#endif