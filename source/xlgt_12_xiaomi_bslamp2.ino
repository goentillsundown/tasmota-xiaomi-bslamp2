#ifdef FIRMWARE_XIAOMI_BSLAMP2

#define XLGT_12 12


/*
 * Night-mode state is managed by the Xiaomi Bedside Lamp 2 front-panel XDRV.
 */
extern bool XiaomiBSLamp2NightMode;


/*********************************************************************************************\
 * Xiaomi Mijia Bedside Lamp 2 - Light driver
 *
 * Physical LED outputs:
 *
 *   PWM1 / GPIO13 -> Red
 *   PWM2 / GPIO14 -> Green
 *   PWM3 / GPIO5  -> Blue
 *   PWM4 / GPIO12 -> White
 *
 * Master LED enables:
 *
 *   GPIO33 -> LED enable 1
 *   GPIO4  -> LED enable 2
 *
 * Both enable outputs must be HIGH for the lamp LEDs to operate.
\*********************************************************************************************/


/*********************************************************************************************\
 * Apply Tasmota light channels to the Xiaomi LED hardware
 *
 * Tasmota exposes the lamp as an RGB + virtual CW/WW light.
 *
 * RGB values are translated through the reverse-engineered Xiaomi color
 * conversion algorithm.
 *
 * Virtual CW/WW values are translated through the reverse-engineered Xiaomi
 * color-temperature conversion tables.
 *
 * Night-light mode bypasses the normal conversion logic and applies the
 * calibrated physical PWM values measured from the original Xiaomi firmware.
\*********************************************************************************************/

bool XiaomiBSLamp2SetChannels(void) {

  uint8_t *channels =
      (uint8_t*)XdrvMailbox.data;


  XiaomiBSLamp2::RGBW output = {
    0.0f,
    0.0f,
    0.0f,
    0.0f
  };


  /*
   * Determine whether Tasmota is requesting any visible light output.
   *
   * RGB channels use their maximum value as the logical level.
   *
   * Virtual CW + WW channels represent a single logical white-light
   * brightness, so their combined value is limited to 255.
   */
  uint16_t logical_level =
      channels[0];


  if (channels[1] > logical_level) {
    logical_level = channels[1];
  }

  if (channels[2] > logical_level) {
    logical_level = channels[2];
  }


  uint16_t ct_level =
      (uint16_t)channels[3] +
      (uint16_t)channels[4];


  if (ct_level > 255) {
    ct_level = 255;
  }


  if (ct_level > logical_level) {
    logical_level = ct_level;
  }


  /*
   * If another Tasmota interface changes the light to a normal brightness
   * while night mode is active, leave night mode automatically.
   *
   * ESPHome treats brightness below approximately 0.012 as the special
   * night-light range. A threshold of 4/255 allows for integer rounding.
   */
  if (XiaomiBSLamp2NightMode &&
      (logical_level > 4)) {

    XiaomiBSLamp2NightMode = false;

    AddLog(
        LOG_LEVEL_INFO,
        PSTR("BSL: Night mode cancelled")
    );
  }


  /*******************************************************************************************\
   * Night-light mode
  \*******************************************************************************************/

  if (XiaomiBSLamp2NightMode &&
      (logical_level > 0)) {

    /*
     * Default night-light calibration measured from the original Xiaomi
     * firmware.
     *
     * These values are physical PWM duty cycles, not RGB color coordinates.
     */
    output.red   = 0.968f;
    output.green = 0.968f;
    output.blue  = 0.972f;
    output.white = 0.0f;
  }


  /*******************************************************************************************\
   * RGB mode
  \*******************************************************************************************/

  else if ((channels[0] > 0) ||
           (channels[1] > 0) ||
           (channels[2] > 0)) {

    uint8_t max_rgb =
        channels[0];


    if (channels[1] > max_rgb) {
      max_rgb = channels[1];
    }

    if (channels[2] > max_rgb) {
      max_rgb = channels[2];
    }


    float brightness =
        max_rgb / 255.0f;


    float red =
        channels[0] / (float)max_rgb;

    float green =
        channels[1] / (float)max_rgb;

    float blue =
        channels[2] / (float)max_rgb;


    output =
        XiaomiBSLamp2::rgbColor(
            red,
            green,
            blue,
            brightness
        );
  }


  /*******************************************************************************************\
   * Color-temperature mode
  \*******************************************************************************************/

  else if ((channels[3] > 0) ||
           (channels[4] > 0)) {

    float brightness =
        ((float)channels[3] +
         (float)channels[4]) /
        255.0f;


    float temperature =
        static_cast<float>(
            light_state.getCT()
        );


    output =
        XiaomiBSLamp2::colorTemperature(
            temperature,
            brightness
        );
  }


  /*******************************************************************************************\
   * Convert normalized output values to Tasmota PWM range
  \*******************************************************************************************/

  uint16_t pwm_red =
      static_cast<uint16_t>(
          output.red *
          Settings->pwm_range
      );

  uint16_t pwm_green =
      static_cast<uint16_t>(
          output.green *
          Settings->pwm_range
      );

  uint16_t pwm_blue =
      static_cast<uint16_t>(
          output.blue *
          Settings->pwm_range
      );

  uint16_t pwm_white =
      static_cast<uint16_t>(
          output.white *
          Settings->pwm_range
      );


  /*
   * Physical PWM mapping:
   *
   *   PWM1 -> GPIO13 -> red
   *   PWM2 -> GPIO14 -> green
   *   PWM3 -> GPIO5  -> blue
   *   PWM4 -> GPIO12 -> white
   */
  TasmotaGlobal.pwm_value[0] =
      pwm_red;

  TasmotaGlobal.pwm_value[1] =
      pwm_green;

  TasmotaGlobal.pwm_value[2] =
      pwm_blue;

  TasmotaGlobal.pwm_value[3] =
      pwm_white;


  PwmApplyGPIO(false);


  /*******************************************************************************************\
   * Master LED enable outputs
  \*******************************************************************************************/

  bool light_on =
      (logical_level > 0);


  digitalWrite(
      XIAOMI_BSLAMP2_GPIO_LED_ENABLE_1,
      light_on ? HIGH : LOW
  );


  digitalWrite(
      XIAOMI_BSLAMP2_GPIO_LED_ENABLE_2,
      light_on ? HIGH : LOW
  );


  return true;
}


/*********************************************************************************************\
 * Tasmota XLGT interface
\*********************************************************************************************/

bool Xlgt12(uint32_t function) {

  bool result =
      false;


  switch (function) {

    case FUNC_MODULE_INIT:

      /*
       * Present the lamp to Tasmota as RGB + CW + WW.
       *
       * The physical lamp has four PWM outputs, but SetOption106 provides the
       * virtual CT representation that is translated by this driver.
       */
      TasmotaGlobal.light_type =
          LT_RGBWC;


      TasmotaGlobal.light_driver =
          XLGT_12;


      pinMode(
          XIAOMI_BSLAMP2_GPIO_LED_ENABLE_1,
          OUTPUT
      );


      pinMode(
          XIAOMI_BSLAMP2_GPIO_LED_ENABLE_2,
          OUTPUT
      );


      /*
       * Keep the LED power stage disabled until Tasmota applies a valid
       * light state.
       */
      digitalWrite(
          XIAOMI_BSLAMP2_GPIO_LED_ENABLE_1,
          LOW
      );


      digitalWrite(
          XIAOMI_BSLAMP2_GPIO_LED_ENABLE_2,
          LOW
      );


      AddLog(
          LOG_LEVEL_INFO,
          PSTR("BSL: Light driver initialized")
      );


      break;


    case FUNC_SET_CHANNELS:

      result =
          XiaomiBSLamp2SetChannels();


      break;
  }


  return result;
}

#endif  // FIRMWARE_XIAOMI_BSLAMP2