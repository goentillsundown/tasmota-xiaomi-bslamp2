#ifdef FIRMWARE_XIAOMI_BSLAMP2

#include <Wire.h>

#define XDRV_124 124


/*********************************************************************************************\
 * Xiaomi Mijia Bedside Lamp 2 - Front panel driver
 *
 * Hardware:
 *
 *   I2C address : 0x2C
 *   SDA         : GPIO21
 *   SCL         : GPIO19
 *   IRQ         : GPIO16
 *
 * The panel pulls GPIO16 LOW when an event is available.
\*********************************************************************************************/

static const uint8_t XIAOMI_BSLAMP2_PANEL_ADDRESS = 0x2C;
static const uint8_t XIAOMI_BSLAMP2_PANEL_MSG_LEN = 7;

static const uint32_t XIAOMI_BSLAMP2_PANEL_COOLDOWN_MS = 5;
static const uint32_t XIAOMI_BSLAMP2_PANEL_RETRY_MS = 5;

static const uint8_t XIAOMI_BSLAMP2_PANEL_WRITE_FAILURE_LIMIT = 20;


/*********************************************************************************************\
 * Front-panel LED masks
 *
 * LED_1 is closest to the power button.
 * LED_10 is closest to the color button.
\*********************************************************************************************/

static const uint16_t XIAOMI_BSLAMP2_LED_POWER = 0x4000;
static const uint16_t XIAOMI_BSLAMP2_LED_COLOR = 0x1000;

static const uint16_t XIAOMI_BSLAMP2_LED_1  = 0x0200;
static const uint16_t XIAOMI_BSLAMP2_LED_2  = 0x0100;
static const uint16_t XIAOMI_BSLAMP2_LED_3  = 0x0080;
static const uint16_t XIAOMI_BSLAMP2_LED_4  = 0x0040;
static const uint16_t XIAOMI_BSLAMP2_LED_5  = 0x0020;
static const uint16_t XIAOMI_BSLAMP2_LED_6  = 0x0010;
static const uint16_t XIAOMI_BSLAMP2_LED_7  = 0x0008;
static const uint16_t XIAOMI_BSLAMP2_LED_8  = 0x0004;
static const uint16_t XIAOMI_BSLAMP2_LED_9  = 0x0002;
static const uint16_t XIAOMI_BSLAMP2_LED_10 = 0x0001;

/*
 * Xiaomi panel protocol control bits.
 */
static const uint16_t XIAOMI_BSLAMP2_LED_CONTROL_BITS = 0x0C00;


/*********************************************************************************************\
 * Front-panel event types
\*********************************************************************************************/

enum XiaomiBSLamp2PanelPart {
  XIAOMI_BSLAMP2_PART_UNKNOWN = 0,
  XIAOMI_BSLAMP2_PART_POWER,
  XIAOMI_BSLAMP2_PART_COLOR,
  XIAOMI_BSLAMP2_PART_SLIDER
};


enum XiaomiBSLamp2PanelEventType {
  XIAOMI_BSLAMP2_EVENT_UNKNOWN = 0,
  XIAOMI_BSLAMP2_EVENT_TOUCH,
  XIAOMI_BSLAMP2_EVENT_RELEASE
};


struct XiaomiBSLamp2PanelEvent {
  bool valid;
  XiaomiBSLamp2PanelPart part;
  XiaomiBSLamp2PanelEventType type;
  uint8_t level;
};


/*********************************************************************************************\
 * Explicit function declarations
 *
 * Explicit declarations avoid Arduino-generated prototypes being placed before
 * the custom event types have been declared.
\*********************************************************************************************/

void XiaomiBSLamp2PanelISR(void) IRAM_ATTR;

XiaomiBSLamp2PanelEvent XiaomiBSLamp2ParsePanelEvent(
    const uint8_t *msg
);

void XiaomiBSLamp2ApplyRgbPreset(void);

void XiaomiBSLamp2ApplyWhitePreset(void);

void XiaomiBSLamp2ProcessPanelEvent(
    const XiaomiBSLamp2PanelEvent &event
);

void XiaomiBSLamp2PanelInit(void);

void XiaomiBSLamp2PanelReadEvent(void);

void XiaomiBSLamp2PanelUpdateLeds(void);

void XiaomiBSLamp2PanelLoop(void);

bool Xdrv124(uint32_t function);


/*********************************************************************************************\
 * Interrupt state
\*********************************************************************************************/

volatile uint32_t XiaomiBSLamp2PanelEventId = 0;

uint32_t XiaomiBSLamp2PanelLastEventId = 0;
uint32_t XiaomiBSLamp2PanelLastEventTime = 0;


/*********************************************************************************************\
 * Front-panel LED state
 *
 * Only the most recent desired LED state needs to be retained. Intermediate
 * states can safely be discarded if the panel is temporarily busy.
\*********************************************************************************************/

uint16_t XiaomiBSLamp2PanelLastLedState = 0xFFFF;

uint16_t XiaomiBSLamp2PanelPendingLedState = 0;

bool XiaomiBSLamp2PanelLedUpdatePending = false;

uint32_t XiaomiBSLamp2PanelLastWriteAttemptTime = 0;

uint8_t XiaomiBSLamp2PanelWriteFailureCount = 0;

bool XiaomiBSLamp2PanelWriteFaultLogged = false;


/*********************************************************************************************\
 * Button and mode state
\*********************************************************************************************/

uint32_t XiaomiBSLamp2ColorTouchTime = 0;
uint32_t XiaomiBSLamp2PowerTouchTime = 0;

bool XiaomiBSLamp2NightMode = false;

bool XiaomiBSLamp2ColorWhiteGroup = false;

uint8_t XiaomiBSLamp2RgbPreset = 0;
uint8_t XiaomiBSLamp2WhitePreset = 0;


/*********************************************************************************************\
 * Interrupt handler
 *
 * I2C communication must not be performed from the ISR. The interrupt only
 * increments an event counter; the main loop reads the actual panel event.
\*********************************************************************************************/

void XiaomiBSLamp2PanelISR(void) {
  XiaomiBSLamp2PanelEventId++;
}


/*********************************************************************************************\
 * Parse a seven-byte front-panel message
\*********************************************************************************************/

XiaomiBSLamp2PanelEvent XiaomiBSLamp2ParsePanelEvent(
    const uint8_t *msg
) {

  XiaomiBSLamp2PanelEvent event = {
    false,
    XIAOMI_BSLAMP2_PART_UNKNOWN,
    XIAOMI_BSLAMP2_EVENT_UNKNOWN,
    0
  };


  /*
   * All supported front-panel events begin with:
   *
   *   04 04 01 00
   */
  if ((msg[0] != 0x04) ||
      (msg[1] != 0x04) ||
      (msg[2] != 0x01) ||
      (msg[3] != 0x00)) {

    AddLog(
        LOG_LEVEL_ERROR,
        PSTR("BSL: Invalid front panel event prefix")
    );

    return event;
  }


  switch (msg[4]) {

    // -------------------------------------------------------------------------
    // Power button
    // -------------------------------------------------------------------------

    case 0x01:

      event.part = XIAOMI_BSLAMP2_PART_POWER;

      if ((msg[5] == 0x01) &&
          (msg[6] == 0x03)) {

        event.type = XIAOMI_BSLAMP2_EVENT_TOUCH;
        event.valid = true;

      } else if ((msg[5] == 0x02) &&
                 (msg[6] == 0x04)) {

        event.type = XIAOMI_BSLAMP2_EVENT_RELEASE;
        event.valid = true;
      }

      break;


    // -------------------------------------------------------------------------
    // Color button
    // -------------------------------------------------------------------------

    case 0x02:

      event.part = XIAOMI_BSLAMP2_PART_COLOR;

      if ((msg[5] == 0x01) &&
          (msg[6] == 0x04)) {

        event.type = XIAOMI_BSLAMP2_EVENT_TOUCH;
        event.valid = true;

      } else if ((msg[5] == 0x02) &&
                 (msg[6] == 0x05)) {

        event.type = XIAOMI_BSLAMP2_EVENT_RELEASE;
        event.valid = true;
      }

      break;


    // -------------------------------------------------------------------------
    // Slider touch
    // -------------------------------------------------------------------------

    case 0x03:

      event.part = XIAOMI_BSLAMP2_PART_SLIDER;
      event.type = XIAOMI_BSLAMP2_EVENT_TOUCH;

      if ((msg[5] >= 0x01) &&
          (msg[5] <= 0x16) &&
          (msg[6] ==
              (uint8_t)(msg[5] + msg[4] + 0x01))) {

        event.level = 0x17 - msg[5];
        event.valid = true;
      }

      break;


    // -------------------------------------------------------------------------
    // Slider release
    // -------------------------------------------------------------------------

    case 0x04:

      event.part = XIAOMI_BSLAMP2_PART_SLIDER;
      event.type = XIAOMI_BSLAMP2_EVENT_RELEASE;

      if ((msg[5] >= 0x01) &&
          (msg[5] <= 0x16) &&
          (msg[6] ==
              (uint8_t)(msg[5] + msg[4] + 0x01))) {

        event.level = 0x17 - msg[5];
        event.valid = true;
      }

      break;


    default:

      break;
  }


  if (!event.valid) {

    AddLog(
        LOG_LEVEL_ERROR,
        PSTR(
            "BSL: Unsupported front panel event "
            "%02X:%02X:%02X"
        ),
        msg[4],
        msg[5],
        msg[6]
    );
  }


  return event;
}


/*********************************************************************************************\
 * Apply RGB preset
 *
 * Presets match the default ESPHome Xiaomi Bedside Lamp 2 configuration:
 *
 *   0 - red
 *   1 - green
 *   2 - blue
 *   3 - yellow
 *   4 - purple
\*********************************************************************************************/

void XiaomiBSLamp2ApplyRgbPreset(void) {

  uint8_t bri =
      (uint8_t)(((uint32_t)light_state.getBri() * 100 + 127) / 255);


  if (bri < 1) {
    bri = 1;
  }


  char command[32];


  switch (XiaomiBSLamp2RgbPreset) {

    case 0:

      snprintf_P(
          command,
          sizeof(command),
          PSTR("HSBColor 0,100,%u"),
          bri
      );

      break;


    case 1:

      snprintf_P(
          command,
          sizeof(command),
          PSTR("HSBColor 120,100,%u"),
          bri
      );

      break;


    case 2:

      snprintf_P(
          command,
          sizeof(command),
          PSTR("HSBColor 240,100,%u"),
          bri
      );

      break;


    case 3:

      snprintf_P(
          command,
          sizeof(command),
          PSTR("HSBColor 60,100,%u"),
          bri
      );

      break;


    default:

      snprintf_P(
          command,
          sizeof(command),
          PSTR("HSBColor 300,100,%u"),
          bri
      );

      break;
  }


  ExecuteCommand(
      command,
      SRC_BUTTON
  );


  ExecuteCommandPower(
      LightDevice(),
      POWER_ON,
      SRC_BUTTON
  );
}


/*********************************************************************************************\
 * Apply white / color-temperature preset
\*********************************************************************************************/

void XiaomiBSLamp2ApplyWhitePreset(void) {

  /*
   * Tasmota currently exposes the standard CT range up to 500 mired.
   *
   * The Xiaomi hardware itself supports up to approximately 588 mired.
   */
  static const uint16_t ct_presets[] = {
    153,
    275,
    400,
    500
  };


  uint8_t bri =
      (uint8_t)(((uint32_t)light_state.getBri() * 100 + 127) / 255);


  if (bri < 1) {
    bri = 1;
  }


  char command[24];


  snprintf_P(
      command,
      sizeof(command),
      PSTR("CT %u"),
      ct_presets[XiaomiBSLamp2WhitePreset]
  );


  ExecuteCommand(
      command,
      SRC_BUTTON
  );


  snprintf_P(
      command,
      sizeof(command),
      PSTR("Dimmer %u"),
      bri
  );


  ExecuteCommand(
      command,
      SRC_BUTTON
  );


  ExecuteCommandPower(
      LightDevice(),
      POWER_ON,
      SRC_BUTTON
  );
}


/*********************************************************************************************\
 * Process a parsed front-panel event
\*********************************************************************************************/

void XiaomiBSLamp2ProcessPanelEvent(
    const XiaomiBSLamp2PanelEvent &event
) {

  if (!event.valid) {
    return;
  }


  switch (event.part) {

    // -------------------------------------------------------------------------
    // Power button
    //
    // Short press:
    //   Toggle normal lamp power.
    //
    // Long press >= 800 ms:
    //   Enable Xiaomi night-light mode.
    // -------------------------------------------------------------------------

    case XIAOMI_BSLAMP2_PART_POWER:

      if (event.type ==
          XIAOMI_BSLAMP2_EVENT_TOUCH) {

        XiaomiBSLamp2PowerTouchTime =
            millis();


      } else if (event.type ==
                 XIAOMI_BSLAMP2_EVENT_RELEASE) {

        uint32_t press_time =
            millis() -
            XiaomiBSLamp2PowerTouchTime;


        if (press_time >= 800) {

          /*
           * Set Tasmota to its minimum logical brightness first.
           *
           * The XLGT light driver then substitutes the calibrated physical
           * Xiaomi night-light PWM values.
           */
          ExecuteCommand(
              PSTR("Dimmer 1"),
              SRC_BUTTON
          );


          XiaomiBSLamp2NightMode = true;


          /*
           * Keep Tasmota logically in color-temperature mode while the
           * special physical night-light output is active.
           */
          ExecuteCommand(
              PSTR("CT 153"),
              SRC_BUTTON
          );


          ExecuteCommandPower(
              LightDevice(),
              POWER_ON,
              SRC_BUTTON
          );


          AddLog(
              LOG_LEVEL_INFO,
              PSTR("BSL: Night mode activated")
          );


        } else {

          ExecuteCommandPower(
              LightDevice(),
              POWER_TOGGLE,
              SRC_BUTTON
          );
        }
      }

      break;


    // -------------------------------------------------------------------------
    // Color button
    //
    // Short press:
    //   Cycle through presets in the current preset group.
    //
    // Long press >= 600 ms:
    //   Switch between RGB and white preset groups.
    // -------------------------------------------------------------------------

    case XIAOMI_BSLAMP2_PART_COLOR:

      if (event.type ==
          XIAOMI_BSLAMP2_EVENT_TOUCH) {

        XiaomiBSLamp2ColorTouchTime =
            millis();


      } else if (event.type ==
                 XIAOMI_BSLAMP2_EVENT_RELEASE) {

        /*
         * Any color-button action exits night-light mode.
         */
        XiaomiBSLamp2NightMode = false;


        uint32_t press_time =
            millis() -
            XiaomiBSLamp2ColorTouchTime;


        if (press_time >= 600) {

          XiaomiBSLamp2ColorWhiteGroup =
              !XiaomiBSLamp2ColorWhiteGroup;


          if (XiaomiBSLamp2ColorWhiteGroup) {

            XiaomiBSLamp2WhitePreset = 0;

            XiaomiBSLamp2ApplyWhitePreset();

          } else {

            XiaomiBSLamp2RgbPreset = 0;

            XiaomiBSLamp2ApplyRgbPreset();
          }


        } else {

          if (XiaomiBSLamp2ColorWhiteGroup) {

            XiaomiBSLamp2WhitePreset++;

            if (XiaomiBSLamp2WhitePreset >= 4) {
              XiaomiBSLamp2WhitePreset = 0;
            }


            XiaomiBSLamp2ApplyWhitePreset();


          } else {

            XiaomiBSLamp2RgbPreset++;

            if (XiaomiBSLamp2RgbPreset >= 5) {
              XiaomiBSLamp2RgbPreset = 0;
            }


            XiaomiBSLamp2ApplyRgbPreset();
          }
        }
      }

      break;


    // -------------------------------------------------------------------------
    // Slider
    //
    // Levels 2..21 are mapped to 20 brightness steps from 2..100 percent.
    // Level 1 is treated like the lowest usable level, matching the ESPHome
    // implementation's handling of the difficult-to-reach first position.
    // -------------------------------------------------------------------------

    case XIAOMI_BSLAMP2_PART_SLIDER:

      if (event.type ==
          XIAOMI_BSLAMP2_EVENT_TOUCH) {

        /*
         * Moving the slider exits night-light mode.
         */
        XiaomiBSLamp2NightMode = false;


        uint8_t corrected_level;


        if (event.level <= 2) {
          corrected_level = 0;
        } else {
          corrected_level = event.level - 2;
        }


        if (corrected_level > 19) {
          corrected_level = 19;
        }


        uint8_t dimmer =
            2 + ((corrected_level * 98) / 19);


        ExecuteCommandPower(
            LightDevice(),
            POWER_ON,
            SRC_BUTTON
        );


        char command[24];


        snprintf_P(
            command,
            sizeof(command),
            PSTR("Dimmer %u"),
            dimmer
        );


        ExecuteCommand(
            command,
            SRC_BUTTON
        );
      }

      break;


    default:

      break;
  }
}


/*********************************************************************************************\
 * Update front-panel illumination
 *
 * LED command:
 *
 *   02 03 HH LL 64 00 00
 *
 * HH:LL contains the 16-bit panel LED state.
 *
 * Normal mode:
 *   - power button illuminated
 *   - color button illuminated
 *   - slider LEDs represent current brightness
 *
 * Night mode:
 *   - only the power button is illuminated
 *
 * Off:
 *   - all visible panel LEDs are off
 *
 * LED writes are deferred rather than retried synchronously. Only the newest
 * required LED state is retained. If the panel is busy, the pending state is
 * attempted again on a later pass through FUNC_LOOP.
\*********************************************************************************************/

void XiaomiBSLamp2PanelUpdateLeds(void) {

  uint32_t light_device =
      LightDevice();


  if (light_device == 0) {
    return;
  }


  bool light_on =
      bitRead(
          TasmotaGlobal.power,
          light_device - 1
      );


  uint16_t led_state =
      XIAOMI_BSLAMP2_LED_CONTROL_BITS;


  if (light_on) {

    if (XiaomiBSLamp2NightMode) {

      /*
       * Original night-light panel behavior.
       */
      led_state |=
          XIAOMI_BSLAMP2_LED_POWER;


    } else {

      /*
       * Normal operating mode.
       */
      led_state |=
          XIAOMI_BSLAMP2_LED_POWER;

      led_state |=
          XIAOMI_BSLAMP2_LED_COLOR;


      float brightness =
          light_state.getBri() / 255.0f;


      /*
       * Thresholds match the reverse-engineered original panel behavior.
       */
      if (brightness > 0.00f) {
        led_state |= XIAOMI_BSLAMP2_LED_1;
      }

      if (brightness > 0.15f) {
        led_state |= XIAOMI_BSLAMP2_LED_2;
      }

      if (brightness > 0.25f) {
        led_state |= XIAOMI_BSLAMP2_LED_3;
      }

      if (brightness > 0.35f) {
        led_state |= XIAOMI_BSLAMP2_LED_4;
      }

      if (brightness > 0.45f) {
        led_state |= XIAOMI_BSLAMP2_LED_5;
      }

      if (brightness > 0.55f) {
        led_state |= XIAOMI_BSLAMP2_LED_6;
      }

      if (brightness > 0.65f) {
        led_state |= XIAOMI_BSLAMP2_LED_7;
      }

      if (brightness > 0.75f) {
        led_state |= XIAOMI_BSLAMP2_LED_8;
      }

      if (brightness > 0.85f) {
        led_state |= XIAOMI_BSLAMP2_LED_9;
      }

      if (brightness > 0.95f) {
        led_state |= XIAOMI_BSLAMP2_LED_10;
      }
    }
  }


  /*
   * If the requested state is already physically present on the panel,
   * discard any older pending state.
   */
  if (led_state ==
      XiaomiBSLamp2PanelLastLedState) {

    XiaomiBSLamp2PanelLedUpdatePending = false;

    return;
  }


  /*
   * Retain only the newest requested LED state. There is no benefit in
   * sending intermediate brightness states after the panel becomes free.
   */
  XiaomiBSLamp2PanelPendingLedState =
      led_state;

  XiaomiBSLamp2PanelLedUpdatePending =
      true;


  /*
   * Do not start another panel transaction immediately after reading an
   * input event.
   */
  if ((XiaomiBSLamp2PanelLastEventTime != 0) &&
      ((uint32_t)(millis() - XiaomiBSLamp2PanelLastEventTime) <
       XIAOMI_BSLAMP2_PANEL_COOLDOWN_MS)) {

    return;
  }


  /*
   * A LOW IRQ means that the panel has an input event waiting to be read.
   * Input events take priority over cosmetic LED updates.
   */
  if (digitalRead(
          XIAOMI_BSLAMP2_GPIO_PANEL_IRQ
      ) == LOW) {

    return;
  }


  /*
   * Limit deferred write attempts so a temporarily busy panel cannot cause
   * continuous I2C traffic from FUNC_LOOP.
   */
  uint32_t now =
      millis();


  if ((XiaomiBSLamp2PanelLastWriteAttemptTime != 0) &&
      ((uint32_t)(now - XiaomiBSLamp2PanelLastWriteAttemptTime) <
       XIAOMI_BSLAMP2_PANEL_RETRY_MS)) {

    return;
  }


  XiaomiBSLamp2PanelLastWriteAttemptTime =
      now;


  uint8_t led_message[
      XIAOMI_BSLAMP2_PANEL_MSG_LEN
  ] = {
    0x02,
    0x03,
    (uint8_t)(XiaomiBSLamp2PanelPendingLedState >> 8),
    (uint8_t)(XiaomiBSLamp2PanelPendingLedState & 0xFF),
    0x64,
    0x00,
    0x00
  };


  Wire.beginTransmission(
      XIAOMI_BSLAMP2_PANEL_ADDRESS
  );


  Wire.write(
      led_message,
      XIAOMI_BSLAMP2_PANEL_MSG_LEN
  );


  uint8_t error =
      Wire.endTransmission();


  if (error != 0) {

    /*
     * Keep the new state pending. A later FUNC_LOOP pass will try again.
     *
     * A single busy response is expected to be transient and is therefore
     * not logged as an error. Only a persistent sequence of failures is
     * considered a bus fault.
     */
    if (XiaomiBSLamp2PanelWriteFailureCount < 0xFF) {
      XiaomiBSLamp2PanelWriteFailureCount++;
    }


    if ((XiaomiBSLamp2PanelWriteFailureCount >=
         XIAOMI_BSLAMP2_PANEL_WRITE_FAILURE_LIMIT) &&
        !XiaomiBSLamp2PanelWriteFaultLogged) {

      AddLog(
          LOG_LEVEL_ERROR,
          PSTR(
              "BSL: Front panel LED writes repeatedly failed, "
              "error=%u"
          ),
          error
      );


      XiaomiBSLamp2PanelWriteFaultLogged =
          true;
    }


    return;
  }


  /*
   * The panel accepted the newest pending state.
   */
  XiaomiBSLamp2PanelLastLedState =
      XiaomiBSLamp2PanelPendingLedState;

  XiaomiBSLamp2PanelLedUpdatePending =
      false;

  XiaomiBSLamp2PanelWriteFailureCount =
      0;

  XiaomiBSLamp2PanelWriteFaultLogged =
      false;
}


/*********************************************************************************************\
 * Initialise front-panel hardware
\*********************************************************************************************/

void XiaomiBSLamp2PanelInit(void) {

  Wire.begin(
      XIAOMI_BSLAMP2_GPIO_PANEL_SDA,
      XIAOMI_BSLAMP2_GPIO_PANEL_SCL
  );


  pinMode(
      XIAOMI_BSLAMP2_GPIO_PANEL_IRQ,
      INPUT_PULLUP
  );


  attachInterrupt(
      digitalPinToInterrupt(
          XIAOMI_BSLAMP2_GPIO_PANEL_IRQ
      ),
      XiaomiBSLamp2PanelISR,
      FALLING
  );


  /*
   * Force an initial panel LED update.
   */
  XiaomiBSLamp2PanelLastLedState =
      0xFFFF;

  XiaomiBSLamp2PanelLedUpdatePending =
      false;


  AddLog(
      LOG_LEVEL_INFO,
      PSTR("BSL: Front panel initialized")
  );
}


/*********************************************************************************************\
 * Read and process one front-panel event
\*********************************************************************************************/

void XiaomiBSLamp2PanelReadEvent(void) {

  static const uint8_t ready_for_event[
      XIAOMI_BSLAMP2_PANEL_MSG_LEN
  ] = {
    0x01,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x01
  };


  /*
   * Tell the panel that the ESP is ready to receive the pending event.
   */
  Wire.beginTransmission(
      XIAOMI_BSLAMP2_PANEL_ADDRESS
  );


  Wire.write(
      ready_for_event,
      XIAOMI_BSLAMP2_PANEL_MSG_LEN
  );


  uint8_t error =
      Wire.endTransmission();


  if (error != 0) {

    AddLog(
        LOG_LEVEL_ERROR,
        PSTR("BSL: Front panel I2C write failed, error=%u"),
        error
    );

    return;
  }


  /*
   * Read exactly seven event bytes.
   */
  uint8_t count =
      Wire.requestFrom(
          XIAOMI_BSLAMP2_PANEL_ADDRESS,
          XIAOMI_BSLAMP2_PANEL_MSG_LEN
      );


  if (count !=
      XIAOMI_BSLAMP2_PANEL_MSG_LEN) {

    AddLog(
        LOG_LEVEL_ERROR,
        PSTR("BSL: Front panel I2C read failed, bytes=%u"),
        count
    );


    while (Wire.available()) {
      Wire.read();
    }

    return;
  }


  uint8_t msg[
      XIAOMI_BSLAMP2_PANEL_MSG_LEN
  ];


  for (uint8_t i = 0;
       i < XIAOMI_BSLAMP2_PANEL_MSG_LEN;
       i++) {

    msg[i] = Wire.read();
  }


  XiaomiBSLamp2PanelEvent event =
      XiaomiBSLamp2ParsePanelEvent(msg);


  XiaomiBSLamp2ProcessPanelEvent(event);


  /*
   * Give the front-panel controller a short recovery period before another
   * I2C transaction is attempted.
   */
  XiaomiBSLamp2PanelLastEventTime =
      millis();
}


/*********************************************************************************************\
 * Main-loop service
\*********************************************************************************************/

void XiaomiBSLamp2PanelLoop(void) {

  uint32_t current_event_id =
      XiaomiBSLamp2PanelEventId;


  if (current_event_id !=
      XiaomiBSLamp2PanelLastEventId) {

    XiaomiBSLamp2PanelLastEventId =
        current_event_id;


    XiaomiBSLamp2PanelReadEvent();
  }


  /*
   * Keep front-panel illumination synchronized with changes made through
   * physical controls, the web UI, MQTT, rules or other Tasmota interfaces.
   *
   * LED updates are queued internally. If an input event is pending, the
   * panel is cooling down, or an I2C write is temporarily rejected, the
   * newest LED state remains pending for a later FUNC_LOOP pass.
   */
  XiaomiBSLamp2PanelUpdateLeds();
}


/*********************************************************************************************\
 * Tasmota XDRV interface
\*********************************************************************************************/

bool Xdrv124(uint32_t function) {

  switch (function) {

    case FUNC_INIT:

      XiaomiBSLamp2PanelInit();

      break;


    case FUNC_LOOP:

      XiaomiBSLamp2PanelLoop();

      break;
  }


  return false;
}

#endif  // FIRMWARE_XIAOMI_BSLAMP2