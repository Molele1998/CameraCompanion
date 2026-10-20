#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// Board connections follow the bundled schematic and official Arduino examples.
// This factory test uses the native portrait orientation throughout.
namespace BoardHW {

constexpr int8_t kSpiSck = 1;
constexpr int8_t kSpiMosi = 2;
constexpr int8_t kSpiMiso = 3;
constexpr int8_t kSdCs = 4;
constexpr int8_t kLcdCs = 14;
constexpr int8_t kLcdDc = 15;
constexpr int8_t kLcdReset = 22;
constexpr int8_t kLcdBacklight = 23;
constexpr int8_t kI2cSda = 18;
constexpr int8_t kI2cScl = 19;
constexpr int8_t kTouchReset = 20;
constexpr int8_t kTouchInterrupt = 21;
constexpr int8_t kBatteryAdc = 0;
constexpr int8_t kBootButton = 9;
constexpr uint8_t kTouchAddress = 0x63;
constexpr uint8_t kImuAddress = 0x6B;
constexpr uint16_t kScreenWidth = 172;
constexpr uint16_t kScreenHeight = 320;

struct TouchSample {
  bool down;
  uint16_t x;
  uint16_t y;
};

struct ImuSample {
  // True only when this call has obtained a new complete sample.
  // When false, values and sampledAt retain the last successful sample.
  bool valid;
  float ax, ay, az;       // Acceleration, g; sensor axes, including gravity.
  float gx, gy, gz;       // Angular velocity, degrees per second.
  float temp;             // Sensor die temperature, degrees Celsius.
  uint32_t sampledAt;      // millis() at the most recent successful read.
};

bool beginDisplay();
Arduino_GFX *display();
void setBrightness(uint8_t percent);
bool beginI2C();
bool beginTouch();
TouchSample readTouch();
bool beginIMU();
ImuSample readIMU();
bool imuPresent();

}  // namespace BoardHW
