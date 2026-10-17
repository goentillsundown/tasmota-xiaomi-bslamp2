#pragma once

#include <stdint.h>
#include <math.h>

/*********************************************************************************************\
 * Xiaomi Mijia Bedside Lamp 2 (MJCTD02YL)
 *
 * Light conversion tables and algorithms.
 *
 * Ported from Maurice Makaay's reverse-engineered ESPHome implementation:
 *   mmakaay/esphome-xiaomi_bslamp2
 *
 * The values in this file are measurements of the original Xiaomi firmware
 * and hardware. They are not based on a generic RGBW conversion model.
 *
 * The RGB path intentionally interpolates between level_a and level_b. The
 * current upstream ESPHome development source appears to use level_a for both
 * lookups, which makes the inter-ring interpolation ineffective. This port
 * uses level_b for the second lookup because that matches the ring design and
 * has been verified on the physical lamp.
\*********************************************************************************************/

namespace XiaomiBSLamp2 {

/*********************************************************************************************\
 * Common data types and limits
\*********************************************************************************************/

struct RGBW {
  float red;
  float green;
  float blue;
  float white;
};

struct RGBWTemperatureEntry {
  float mired;
  float red;
  float green;
  float blue;
  float white;
};

static constexpr float MIRED_MIN = 153.0f;
static constexpr float MIRED_MAX = 588.0f;

/*********************************************************************************************\
 * Color-temperature conversion
 *
 * Measured physical PWM levels at 1% and 100% brightness.
\*********************************************************************************************/

// Measured GPIO levels at 1% brightness.
static constexpr RGBWTemperatureEntry CT_LEVELS_1[] = {
  {501.0f, 0.873f, 0.907f, 1.000f, 0.063f},
  {455.0f, 0.873f, 0.896f, 1.000f, 0.063f},
  {417.0f, 0.873f, 0.891f, 1.000f, 0.068f},
  {371.0f, 0.873f, 0.880f, 1.000f, 0.070f},
  {334.0f, 0.873f, 0.887f, 1.000f, 0.088f},
  {313.0f, 0.882f, 0.904f, 1.000f, 0.128f},
  {295.0f, 0.947f, 1.000f, 0.968f, 0.145f},
  {251.0f, 0.999f, 1.000f, 1.000f, 0.155f},
  {223.0f, 1.000f, 0.899f, 0.921f, 0.130f},
  {201.0f, 1.000f, 0.873f, 0.908f, 0.115f},
  {182.0f, 1.000f, 0.873f, 0.901f, 0.103f},
  {173.0f, 1.000f, 0.873f, 0.904f, 0.094f},
  {167.0f, 1.000f, 0.873f, 0.891f, 0.098f},
  {154.0f, 1.000f, 0.873f, 0.894f, 0.090f},
  {153.0f, 1.000f, 0.873f, 0.892f, 0.088f}
};

// Measured GPIO levels at 100% brightness.
static constexpr RGBWTemperatureEntry CT_LEVELS_100[] = {
  {501.0f, 0.000f, 0.344f, 1.000f, 0.068f},
  {455.0f, 0.000f, 0.237f, 1.000f, 0.093f},
  {417.0f, 0.000f, 0.186f, 1.000f, 0.120f},
  {371.0f, 0.000f, 0.149f, 1.000f, 0.167f},
  {334.0f, 0.000f, 0.135f, 1.000f, 0.325f},
  {313.0f, 0.097f, 0.314f, 1.000f, 0.740f},
  {295.0f, 0.745f, 1.000f, 0.953f, 0.905f},
  {251.0f, 1.000f, 1.000f, 1.000f, 1.000f},
  {223.0f, 1.000f, 0.267f, 0.485f, 0.765f},
  {201.0f, 1.000f, 0.000f, 0.355f, 0.609f},
  {182.0f, 1.000f, 0.000f, 0.282f, 0.489f},
  {173.0f, 1.000f, 0.000f, 0.313f, 0.392f},
  {167.0f, 1.000f, 0.000f, 0.180f, 0.422f},
  {154.0f, 1.000f, 0.000f, 0.218f, 0.368f},
  {153.0f, 1.000f, 0.000f, 0.187f, 0.335f}
};

/*********************************************************************************************\
 * Helper functions
\*********************************************************************************************/

static inline float clampf(float value, float minimum, float maximum) {
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return value;
}

static inline float lerpf(float a, float b, float amount) {
  return a + ((b - a) * amount);
}

static inline const RGBWTemperatureEntry &lookupTemperature(
    const RGBWTemperatureEntry *table,
    float temperature) {

  for (uint8_t i = 0; i < 15; i++) {
    if (temperature >= table[i].mired) {
      return table[i];
    }
  }

  return table[14];
}

/**
 * Convert requested color temperature and brightness to the four physical
 * PWM output levels used by the lamp. Active brightness is clamped to a
 * minimum of 1%, matching the source implementation.
 */
static inline RGBW colorTemperature(
    float temperature_mired,
    float brightness) {

  temperature_mired =
      clampf(temperature_mired, MIRED_MIN, MIRED_MAX);

  brightness =
      clampf(brightness, 0.01f, 1.00f);

  const RGBWTemperatureEntry &low =
      lookupTemperature(CT_LEVELS_1, temperature_mired);

  const RGBWTemperatureEntry &high =
      lookupTemperature(CT_LEVELS_100, temperature_mired);

  RGBW result;

  result.red =
      lerpf(low.red, high.red, brightness);

  result.green =
      lerpf(low.green, high.green, brightness);

  result.blue =
      lerpf(low.blue, high.blue, brightness);

  result.white =
      lerpf(low.white, high.white, brightness);

  return result;
}

/*********************************************************************************************\
 * RGB conversion
 *
 * The RGB model is represented by seven measured rings with 24 hue positions
 * per ring. Each point contains measured output levels at 1% and 100%
 * brightness. Ring 7 is calculated as the white center point.
\*********************************************************************************************/

struct RGB {
  float red;
  float green;
  float blue;
};

struct RGBPoint {
  RGB low;
  RGB high;
};

struct RGBRing {
  RGBPoint point[24];
};

struct RGBCircle {
  RGBRing ring[7];
};

static constexpr RGBRing RGB_RING_0 = {{
  {{0.8998f, 0.9997f, 0.9997f}, {0.0000f, 0.9997f, 0.9997f}}, //   0°
  {{0.8727f, 0.9404f, 0.9682f}, {0.0000f, 0.6758f, 0.9539f}}, //  15°
  {{0.8727f, 0.8967f, 0.9677f}, {0.0000f, 0.2389f, 0.9506f}}, //  30°
  {{0.9030f, 0.8727f, 0.9682f}, {0.3040f, 0.0000f, 0.9536f}}, //  45°
  {{0.9270f, 0.8727f, 0.9685f}, {0.5426f, 0.0000f, 0.9570f}}, //  60°
  {{0.9404f, 0.8727f, 0.9687f}, {0.6753f, 0.0000f, 0.9590f}}, //  75°
  {{0.9491f, 0.8727f, 0.9687f}, {0.7638f, 0.0000f, 0.9601f}}, //  90°
  {{0.9539f, 0.8727f, 0.9689f}, {0.8115f, 0.0000f, 0.9609f}}, // 105°
  {{0.9997f, 0.8998f, 0.9997f}, {0.9997f, 0.0000f, 0.9997f}}, // 120°
  {{0.9553f, 0.8727f, 0.9672f}, {0.8264f, 0.0000f, 0.9452f}}, // 135°
  {{0.9555f, 0.8727f, 0.9621f}, {0.8266f, 0.0000f, 0.8937f}}, // 150°
  {{0.9555f, 0.8727f, 0.9524f}, {0.8273f, 0.0000f, 0.7964f}}, // 165°
  {{0.9555f, 0.8727f, 0.9375f}, {0.8285f, 0.0000f, 0.6469f}}, // 180°
  {{0.9557f, 0.8727f, 0.9091f}, {0.8301f, 0.0000f, 0.3648f}}, // 195°
  {{0.9606f, 0.9037f, 0.8727f}, {0.8782f, 0.3091f, 0.0000f}}, // 210°
  {{0.9677f, 0.9514f, 0.8727f}, {0.9486f, 0.7856f, 0.0000f}}, // 225°
  {{0.9997f, 0.9997f, 0.8998f}, {0.9997f, 0.9997f, 0.0000f}}, // 240°
  {{0.9652f, 0.9652f, 0.8727f}, {0.9245f, 0.9252f, 0.0000f}}, // 255°
  {{0.9501f, 0.9631f, 0.8727f}, {0.7746f, 0.9029f, 0.0000f}}, // 270°
  {{0.9219f, 0.9587f, 0.8727f}, {0.4919f, 0.8601f, 0.0000f}}, // 285°
  {{0.8786f, 0.9521f, 0.8727f}, {0.0584f, 0.7946f, 0.0000f}}, // 300°
  {{0.8727f, 0.9531f, 0.9152f}, {0.0000f, 0.8022f, 0.4250f}}, // 315°
  {{0.8727f, 0.9542f, 0.9467f}, {0.0000f, 0.8145f, 0.7889f}}, // 330°
  {{0.8728f, 0.9547f, 0.9631f}, {0.0000f, 0.8207f, 0.9044f}}  // 345°
}};

static constexpr RGBRing RGB_RING_1 = {{
  {{0.8727f, 0.9499f, 0.9660f}, {0.0000f, 0.7714f, 0.9337f}},
  {{0.8727f, 0.9255f, 0.9665f}, {0.0000f, 0.5268f, 0.9365f}},
  {{0.8727f, 0.8793f, 0.9662f}, {0.0000f, 0.0664f, 0.9345f}},
  {{0.9079f, 0.8727f, 0.9672f}, {0.3515f, 0.0000f, 0.9455f}},
  {{0.9270f, 0.8727f, 0.9680f}, {0.5429f, 0.0000f, 0.9519f}},
  {{0.9386f, 0.8727f, 0.9680f}, {0.6593f, 0.0000f, 0.9534f}},
  {{0.9470f, 0.8727f, 0.9682f}, {0.7433f, 0.0000f, 0.9546f}},
  {{0.9524f, 0.8727f, 0.9682f}, {0.7961f, 0.0000f, 0.9552f}},
  {{0.9547f, 0.8727f, 0.9682f}, {0.8212f, 0.0000f, 0.9555f}},
  {{0.9547f, 0.8727f, 0.9655f}, {0.8215f, 0.0000f, 0.9284f}},
  {{0.9550f, 0.8727f, 0.9600f}, {0.8217f, 0.0000f, 0.8712f}},
  {{0.9550f, 0.8727f, 0.9506f}, {0.8225f, 0.0000f, 0.7788f}},
  {{0.9551f, 0.8727f, 0.9375f}, {0.8235f, 0.0000f, 0.6469f}},
  {{0.9547f, 0.8727f, 0.9150f}, {0.8212f, 0.0000f, 0.4226f}},
  {{0.9560f, 0.8828f, 0.8727f}, {0.8322f, 0.1003f, 0.0000f}},
  {{0.9639f, 0.9357f, 0.8727f}, {0.9104f, 0.6289f, 0.0000f}},
  {{0.9675f, 0.9610f, 0.8727f}, {0.9475f, 0.8803f, 0.0000f}},
  {{0.9596f, 0.9597f, 0.8727f}, {0.8686f, 0.8686f, 0.0000f}},
  {{0.9430f, 0.9570f, 0.8727f}, {0.7023f, 0.8435f, 0.0000f}},
  {{0.9160f, 0.9530f, 0.8727f}, {0.4332f, 0.8032f, 0.0000f}},
  {{0.8780f, 0.9472f, 0.8728f}, {0.0518f, 0.7458f, 0.0000f}},
  {{0.8727f, 0.9477f, 0.9099f}, {0.0000f, 0.7497f, 0.3717f}},
  {{0.8727f, 0.9490f, 0.9396f}, {0.0000f, 0.7612f, 0.6689f}},
  {{0.8727f, 0.9496f, 0.9580f}, {0.0000f, 0.7683f, 0.8512f}}
}};

static constexpr RGBRing RGB_RING_2 = {{
  {{0.8727f, 0.9352f, 0.9609f}, {0.0000f, 0.6244f, 0.8822f}},
  {{0.8727f, 0.9035f, 0.9616f}, {0.0000f, 0.3068f, 0.8888f}},
  {{0.8847f, 0.8727f, 0.9629f}, {0.1189f, 0.0000f, 0.9004f}},
  {{0.9121f, 0.8727f, 0.9650f}, {0.3936f, 0.0000f, 0.9237f}},
  {{0.9270f, 0.8727f, 0.9665f}, {0.5431f, 0.0000f, 0.9367f}},
  {{0.9370f, 0.8727f, 0.9665f}, {0.6428f, 0.0000f, 0.9377f}},
  {{0.9445f, 0.8727f, 0.9665f}, {0.7182f, 0.0000f, 0.9386f}},
  {{0.9499f, 0.8727f, 0.9667f}, {0.7722f, 0.0000f, 0.9391f}},
  {{0.9534f, 0.8727f, 0.9667f}, {0.8066f, 0.0000f, 0.9396f}},
  {{0.9534f, 0.8727f, 0.9631f}, {0.8068f, 0.0000f, 0.9024f}},
  {{0.9536f, 0.8727f, 0.9570f}, {0.8073f, 0.0000f, 0.8435f}},
  {{0.9536f, 0.8727f, 0.9487f}, {0.8081f, 0.0000f, 0.7602f}},
  {{0.9536f, 0.8727f, 0.9375f}, {0.8088f, 0.0000f, 0.6474f}},
  {{0.9526f, 0.8727f, 0.9201f}, {0.7990f, 0.0000f, 0.4736f}},
  {{0.9509f, 0.8727f, 0.8866f}, {0.7804f, 0.0000f, 0.1383f}},
  {{0.9557f, 0.9109f, 0.8727f}, {0.8291f, 0.3813f, 0.0000f}},
  {{0.9606f, 0.9455f, 0.8727f}, {0.8798f, 0.7268f, 0.0000f}},
  {{0.9499f, 0.9439f, 0.8727f}, {0.7714f, 0.7107f, 0.0000f}},
  {{0.9329f, 0.9414f, 0.8727f}, {0.6005f, 0.6855f, 0.0000f}},
  {{0.9086f, 0.9377f, 0.8727f}, {0.5381f, 0.6496f, 0.0000f}},
  {{0.8760f, 0.9329f, 0.8727f}, {0.0314f, 0.6013f, 0.0000f}},
  {{0.8727f, 0.9331f, 0.9052f}, {0.0000f, 0.6027f, 0.3248f}},
  {{0.8727f, 0.9340f, 0.9316f}, {0.0000f, 0.6130f, 0.5876f}},
  {{0.8727f, 0.9347f, 0.9499f}, {0.0000f, 0.6202f, 0.7717f}}
}};

static constexpr RGBRing RGB_RING_3 = {{
  {{0.8727f, 0.9114f, 0.9526f}, {0.0000f, 0.3850f, 0.7983f}},
  {{0.8727f, 0.8788f, 0.9541f}, {0.0000f, 0.0615f, 0.8125f}},
  {{0.8979f, 0.8727f, 0.9587f}, {0.2526f, 0.0000f, 0.8586f}},
  {{0.9160f, 0.8727f, 0.9619f}, {0.4311f, 0.0000f, 0.8906f}},
  {{0.9272f, 0.8727f, 0.9639f}, {0.6254f, 0.0000f, 0.9111f}},
  {{0.9352f, 0.8727f, 0.9639f}, {0.6254f, 0.0000f, 0.9111f}},
  {{0.9419f, 0.8727f, 0.9639f}, {0.6909f, 0.0000f, 0.9116f}},
  {{0.9472f, 0.8727f, 0.9639f}, {0.7438f, 0.0000f, 0.9119f}},
  {{0.9509f, 0.8727f, 0.9640f}, {0.7814f, 0.0000f, 0.9121f}},
  {{0.9509f, 0.7827f, 0.9599f}, {0.7819f, 0.0000f, 0.7813f}},
  {{0.9509f, 0.7827f, 0.9541f}, {0.7824f, 0.0000f, 0.8135f}},
  {{0.9511f, 0.7827f, 0.9467f}, {0.7829f, 0.0000f, 0.7409f}},
  {{0.9512f, 0.7827f, 0.9376f}, {0.7837f, 0.0000f, 0.6484f}},
  {{0.9494f, 0.7827f, 0.9245f}, {0.7661f, 0.0000f, 0.5188f}},
  {{0.9465f, 0.7827f, 0.9035f}, {0.7373f, 0.0000f, 0.3073f}},
  {{0.9435f, 0.8810f, 0.8727f}, {0.7077f, 0.0083f, 0.0000f}},
  {{0.9491f, 0.9190f, 0.8727f}, {0.7633f, 0.4624f, 0.0000f}},
  {{0.9372f, 0.9171f, 0.8727f}, {0.6443f, 0.4450f, 0.0000f}},
  {{0.9204f, 0.9147f, 0.8727f}, {0.4760f, 0.4209f, 0.0000f}},
  {{0.8991f, 0.9119f, 0.8727f}, {0.2651f, 0.3906f, 0.0000f}},
  {{0.8727f, 0.9081f, 0.8730f}, {0.0000f, 0.3540f, 0.0034f}},
  {{0.8727f, 0.9094f, 0.9016f}, {0.0000f, 0.3652f, 0.2871f}},
  {{0.8727f, 0.9101f, 0.9236f}, {0.0000f, 0.3737f, 0.5083f}},
  {{0.8727f, 0.9109f, 0.9411f}, {0.0000f, 0.3805f, 0.6833f}}
}};

static constexpr RGBRing RGB_RING_4 = {{
  {{0.8727f, 0.8783f, 0.9416f}, {0.0000f, 0.0566f, 0.6879f}},
  {{0.8916f, 0.8727f, 0.9484f}, {0.1869f, 0.0000f, 0.7575f}},
  {{0.9081f, 0.8727f, 0.9539f}, {0.3550f, 0.0000f, 0.8110f}},
  {{0.9191f, 0.8727f, 0.9572f}, {0.4646f, 0.0000f, 0.8460f}},
  {{0.9272f, 0.8727f, 0.9600f}, {0.5444f, 0.0000f, 0.8712f}},
  {{0.9336f, 0.8727f, 0.9600f}, {0.6074f, 0.0000f, 0.8713f}},
  {{0.9387f, 0.8727f, 0.9600f}, {0.6603f, 0.0000f, 0.8712f}},
  {{0.9435f, 0.8727f, 0.9600f}, {0.7071f, 0.0000f, 0.8712f}},
  {{0.9472f, 0.8727f, 0.9600f}, {0.7442f, 0.0000f, 0.8713f}},
  {{0.9472f, 0.8727f, 0.9557f}, {0.7446f, 0.0000f, 0.8311f}},
  {{0.9472f, 0.8727f, 0.9506f}, {0.7451f, 0.0000f, 0.7798f}},
  {{0.9472f, 0.8727f, 0.9447f}, {0.7456f, 0.0000f, 0.7208f}},
  {{0.9475f, 0.8727f, 0.9377f}, {0.7463f, 0.0000f, 0.6498f}},
  {{0.9450f, 0.8727f, 0.9286f}, {0.7228f, 0.0000f, 0.5588f}},
  {{0.9419f, 0.8727f, 0.9160f}, {0.6904f, 0.0000f, 0.4319f}},
  {{0.9367f, 0.8727f, 0.8960f}, {0.6393f, 0.0000f, 0.2328f}},
  {{0.9316f, 0.8793f, 0.8727f}, {0.5890f, 0.0666f, 0.0000f}},
  {{0.9199f, 0.8778f, 0.8727f}, {0.4716f, 0.0505f, 0.0000f}},
  {{0.9050f, 0.8757f, 0.8727f}, {0.3227f, 0.0003f, 0.0000f}},
  {{0.8878f, 0.8735f, 0.8727f}, {0.1513f, 0.0064f, 0.0000f}},
  {{0.8727f, 0.8760f, 0.8781f}, {0.0000f, 0.0316f, 0.0528f}},
  {{0.8727f, 0.8766f, 0.8990f}, {0.0000f, 0.0398f, 0.2607f}},
  {{0.8727f, 0.8775f, 0.9160f}, {0.0000f, 0.0465f, 0.4316f}},
  {{0.8727f, 0.8778f, 0.9306f}, {0.0000f, 0.0523f, 0.5798f}}
}};

static constexpr RGBRing RGB_RING_5 = {{
  {{0.8980f, 0.8727f, 0.9391f}, {0.5784f, 0.4409f, 0.8030f}},
  {{0.9079f, 0.8727f, 0.9445f}, {0.6330f, 0.4409f, 0.8327f}},
  {{0.9162f, 0.8727f, 0.9486f}, {0.6774f, 0.4409f, 0.8556f}},
  {{0.9221f, 0.8727f, 0.9519f}, {0.7104f, 0.4409f, 0.8725f}},
  {{0.9272f, 0.8727f, 0.9545f}, {0.7382f, 0.4409f, 0.8866f}},
  {{0.9316f, 0.8727f, 0.9545f}, {0.7618f, 0.4409f, 0.8863f}},
  {{0.9355f, 0.8727f, 0.9544f}, {0.7827f, 0.4409f, 0.8861f}},
  {{0.9391f, 0.8727f, 0.9542f}, {0.8025f, 0.4409f, 0.8859f}},
  {{0.9421f, 0.8727f, 0.9542f}, {0.8191f, 0.4409f, 0.8859f}},
  {{0.9421f, 0.8727f, 0.9509f}, {0.8193f, 0.4409f, 0.8676f}},
  {{0.9421f, 0.8727f, 0.9470f}, {0.8195f, 0.4409f, 0.8458f}},
  {{0.9424f, 0.8727f, 0.9429f}, {0.8197f, 0.4409f, 0.8230f}},
  {{0.9424f, 0.8727f, 0.9380f}, {0.8202f, 0.4409f, 0.7961f}},
  {{0.9399f, 0.8727f, 0.9321f}, {0.8071f, 0.4409f, 0.7651f}},
  {{0.9370f, 0.8727f, 0.9252f}, {0.7912f, 0.4409f, 0.7274f}},
  {{0.9331f, 0.8727f, 0.9160f}, {0.7697f, 0.4409f, 0.6767f}},
  {{0.9282f, 0.8727f, 0.9045f}, {0.7431f, 0.4409f, 0.6135f}},
  {{0.9221f, 0.8727f, 0.9050f}, {0.7094f, 0.4409f, 0.6164f}},
  {{0.9147f, 0.8727f, 0.9057f}, {0.6700f, 0.4409f, 0.6200f}},
  {{0.9074f, 0.8727f, 0.9062f}, {0.6296f, 0.4409f, 0.6240f}},
  {{0.8988f, 0.8727f, 0.9072f}, {0.5834f, 0.4409f, 0.6282f}},
  {{0.8986f, 0.8727f, 0.9166f}, {0.5820f, 0.4409f, 0.6807f}},
  {{0.8985f, 0.8727f, 0.9250f}, {0.5805f, 0.4409f, 0.7258f}},
  {{0.8981f, 0.8727f, 0.9329f}, {0.5793f, 0.4409f, 0.7687f}}
}};

static constexpr RGBRing RGB_RING_6 = {{
  {{0.9167f, 0.8727f, 0.9389f}, {0.4399f, 0.0000f, 0.6606f}},
  {{0.9199f, 0.8727f, 0.9414f}, {0.4711f, 0.0000f, 0.6850f}},
  {{0.9226f, 0.8727f, 0.9432f}, {0.4992f, 0.0000f, 0.7068f}},
  {{0.9252f, 0.8727f, 0.9452f}, {0.5241f, 0.0000f, 0.7262f}},
  {{0.9275f, 0.8727f, 0.9472f}, {0.5465f, 0.0000f, 0.7437f}},
  {{0.9294f, 0.8727f, 0.9470f}, {0.5675f, 0.0000f, 0.7432f}},
  {{0.9316f, 0.8727f, 0.9470f}, {0.5879f, 0.0000f, 0.7427f}},
  {{0.9335f, 0.8727f, 0.9470f}, {0.6074f, 0.0000f, 0.7424f}},
  {{0.9352f, 0.8727f, 0.9470f}, {0.6259f, 0.0000f, 0.7419f}},
  {{0.9352f, 0.8727f, 0.9450f}, {0.6262f, 0.0000f, 0.7217f}},
  {{0.9355f, 0.8727f, 0.9429f}, {0.6264f, 0.0000f, 0.7002f}},
  {{0.9355f, 0.8727f, 0.9406f}, {0.6269f, 0.0000f, 0.6777f}},
  {{0.9355f, 0.8727f, 0.9381f}, {0.6272f, 0.0000f, 0.6540f}},
  {{0.9339f, 0.8727f, 0.9357f}, {0.6111f, 0.0000f, 0.6291f}},
  {{0.9321f, 0.8727f, 0.9329f}, {0.5929f, 0.0000f, 0.6013f}},
  {{0.9301f, 0.8727f, 0.9299f}, {0.5729f, 0.0000f, 0.5703f}},
  {{0.9277f, 0.8727f, 0.9263f}, {0.5503f, 0.0000f, 0.5354f}},
  {{0.9252f, 0.8727f, 0.9265f}, {0.5249f, 0.0000f, 0.5368f}},
  {{0.9226f, 0.8727f, 0.9265f}, {0.4985f, 0.0000f, 0.5383f}},
  {{0.9199f, 0.8727f, 0.9267f}, {0.4711f, 0.0000f, 0.5399f}},
  {{0.9170f, 0.8727f, 0.9270f}, {0.4426f, 0.0000f, 0.5414f}},
  {{0.9170f, 0.8727f, 0.9301f}, {0.4419f, 0.0000f, 0.5734f}},
  {{0.9170f, 0.8727f, 0.9331f}, {0.4411f, 0.0000f, 0.6039f}},
  {{0.9167f, 0.8727f, 0.9360f}, {0.4406f, 0.0000f, 0.6330f}}
}};

static constexpr RGBCircle RGB_CIRCLE = {{
  RGB_RING_0,
  RGB_RING_1,
  RGB_RING_2,
  RGB_RING_3,
  RGB_RING_4,
  RGB_RING_5,
  RGB_RING_6
}};

/*********************************************************************************************\
 * RGB conversion helpers
\*********************************************************************************************/

static inline float ringPosition(float red, float green, float blue) {
  float rgb_min = red;

  if (green < rgb_min) rgb_min = green;
  if (blue < rgb_min) rgb_min = blue;

  float rgb_max = red;

  if (green > rgb_max) rgb_max = green;
  if (blue > rgb_max) rgb_max = blue;

  float delta = rgb_max - rgb_min;
  float pos;

  if (delta == 0.0f) {
    pos = 0.0f;
  } else if (red == rgb_max) {
    pos = 60.0f * fmodf((green - blue) / delta, 6.0f);
  } else if (green == rgb_max) {
    pos = 60.0f * (((blue - red) / delta) + 2.0f);
  } else {
    pos = 60.0f * (((red - green) / delta) + 4.0f);
  }

  if (pos < 0.0f) {
    pos += 360.0f;
  }

  return pos;
}

static inline RGB applyRgbBrightness(
    const RGBPoint &point,
    float brightness) {

  RGB result;

  /*
   * This deliberately matches the reverse-engineered ESPHome implementation.
   * Do not normalize this to brightness / 0.99.
   */
  float d = brightness - 0.01f;

  result.red =
      lerpf(point.low.red, point.high.red, d);

  result.green =
      lerpf(point.low.green, point.high.green, d);

  result.blue =
      lerpf(point.low.blue, point.high.blue, d);

  return result;
}

static inline RGB rgbForRing(
    uint8_t ring_level,
    float red,
    float green,
    float blue,
    float brightness) {

  // Ring 7 is the white center point of the RGB color circle.
  if (ring_level >= 7) {
    RGB result;

    result.red   = 0.932101f - (0.383377f * brightness);
    result.green = 0.883185f - (0.881623f * brightness);
    result.blue  = 0.941880f - (0.284498f * brightness);

    return result;
  }

  float pos =
      ringPosition(red, green, blue) / 15.0f;

  uint8_t pos_x =
      static_cast<uint8_t>(floorf(pos));

  uint8_t pos_y =
      static_cast<uint8_t>(ceilf(pos));

  if (pos_y > 23) {
    pos_y = 0;
  }

  const RGBPoint &x =
      RGB_CIRCLE.ring[ring_level].point[pos_x];

  const RGBPoint &y =
      RGB_CIRCLE.ring[ring_level].point[pos_y];

  float d =
      pos - static_cast<float>(pos_x);

  RGBPoint interpolated;

  interpolated.low.red =
      lerpf(x.low.red, y.low.red, d);

  interpolated.low.green =
      lerpf(x.low.green, y.low.green, d);

  interpolated.low.blue =
      lerpf(x.low.blue, y.low.blue, d);

  interpolated.high.red =
      lerpf(x.high.red, y.high.red, d);

  interpolated.high.green =
      lerpf(x.high.green, y.high.green, d);

  interpolated.high.blue =
      lerpf(x.high.blue, y.high.blue, d);

  return applyRgbBrightness(
      interpolated,
      brightness
  );
}

static inline RGBW rgbColor(
    float red,
    float green,
    float blue,
    float brightness) {

  red =
      clampf(red, 0.0f, 1.0f);

  green =
      clampf(green, 0.0f, 1.0f);

  blue =
      clampf(blue, 0.0f, 1.0f);

  brightness =
      clampf(brightness, 0.01f, 1.0f);

  float rgb_min = red;

  if (green < rgb_min) rgb_min = green;
  if (blue < rgb_min) rgb_min = blue;

  float level =
      7.0f * rgb_min;

  uint8_t level_a =
      static_cast<uint8_t>(floorf(level));

  uint8_t level_b =
      level_a + 1;

  if (level_b > 7) {
    level_b = 7;
  }

  RGB rgb_a =
      rgbForRing(
          level_a,
          red,
          green,
          blue,
          brightness
      );

  /*
   * Intentional divergence from the current upstream ESPHome development
   * source:
   *
   * level_b is used for the second ring lookup so interpolation actually
   * occurs between adjacent rings. Using level_a for both lookups makes the
   * interpolation factor below ineffective.
   */
  RGB rgb_b =
      rgbForRing(
          level_b,
          red,
          green,
          blue,
          brightness
      );

  float d =
      level -
      static_cast<float>(level_a);

  RGBW result;

  result.red =
      lerpf(rgb_a.red, rgb_b.red, d);

  result.green =
      lerpf(rgb_a.green, rgb_b.green, d);

  result.blue =
      lerpf(rgb_a.blue, rgb_b.blue, d);

  /*
   * The physical white PWM channel is not used in RGB mode.
   */
  result.white = 0.0f;

  return result;
}

}  // namespace XiaomiBSLamp2