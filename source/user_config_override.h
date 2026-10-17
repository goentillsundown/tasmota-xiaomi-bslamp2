/*
  user_config_override.h - user configuration overrides my_user_config.h for Tasmota

  Copyright (C) 2021  Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _USER_CONFIG_OVERRIDE_H_
#define _USER_CONFIG_OVERRIDE_H_


/*********************************************************************************************\
 * Xiaomi Mijia Bedside Lamp 2 (MJCTD02YL)
 *
 * Custom Tasmota build configuration.
\*********************************************************************************************/

#define FIRMWARE_XIAOMI_BSLAMP2

#include "xiaomi_bslamp2.h"


/*********************************************************************************************\
 * Module template
 *
 * Physical PWM outputs:
 *
 *   GPIO13 -> PWM1 -> Red
 *   GPIO14 -> PWM2 -> Green
 *   GPIO5  -> PWM3 -> Blue
 *   GPIO12 -> PWM4 -> White
 *
 * The front panel and LED enable GPIOs are handled directly by the custom
 * XDRV/XLGT drivers and therefore are not assigned normal Tasmota functions
 * in the module template.
\*********************************************************************************************/

#undef MODULE
#define MODULE USER_MODULE

#undef USER_TEMPLATE
#define USER_TEMPLATE \
  "{\"NAME\":\"Xiaomi Bedside Lamp 2\"," \
  "\"GPIO\":[1,1,1,1,1,418,1,1,419,416,417,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1]," \
  "\"FLAG\":0," \
  "\"BASE\":1}"


#endif  // _USER_CONFIG_OVERRIDE_H_
