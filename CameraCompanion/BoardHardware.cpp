#include "BoardHardware.h"

#if !defined(CONFIG_IDF_TARGET_ESP32C6)
#error "This test targets ESP32-C6. Select ESP32C6 Dev Module in Arduino IDE."
#endif

#include <SPI.h>
#include <Wire.h>

namespace BoardHW {
namespace {

constexpr uint32_t kLcdFrequency = 40000000;
constexpr uint32_t kBacklightFrequency = 5000;
constexpr uint8_t kBacklightResolution = 10;
constexpr uint16_t kBacklightMax = (1U << kBacklightResolution) - 1U;
constexpr uint32_t kTouchPollMs = 10;
constexpr uint32_t kTouchErrorReleaseMs = 120;

// Passing MISO explicitly keeps the SD receive pin on the shared SPI bus.
// The bundled Arduino_ST7789 implementation selects SPI Mode 3 on ESP32.
Arduino_HWSPI g_displayBus(kLcdDc, kLcdCs, kSpiSck, kSpiMosi, kSpiMiso,
                         &SPI, true);
Arduino_ST7789 g_display(&g_displayBus, kLcdReset, 0, false,
                        kScreenWidth, kScreenHeight, 34, 0, 34, 0);
bool g_displayReady = false;
bool g_backlightReady = false;
bool g_i2cReady = false;
bool g_touchReady = false;
bool g_imuPresent = false;
volatile bool g_touchIrq = false;
uint32_t g_lastTouchPoll = 0;
uint32_t g_lastTouchSuccess = 0;
TouchSample g_touch = {false, 0, 0};
ImuSample g_imu = {false, 0, 0, 0, 0, 0, 0, 0, 0};

void ARDUINO_ISR_ATTR touchInterrupt() {
  g_touchIrq = true;
}

bool readRegisters(uint8_t address, uint8_t reg, uint8_t *buffer,
                   size_t length) {
  if (!g_i2cReady || !buffer || length == 0) return false;
  Wire.beginTransmission(address);
  if (Wire.write(reg) != 1) {
    Wire.endTransmission();
    return false;
  }
  // Match the official AXS5106L driver's stop/start register read sequence.
  if (Wire.endTransmission() != 0) return false;
  const size_t received = Wire.requestFrom(address, length);
  if (received != length) {
    while (Wire.available()) Wire.read();
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    if (!Wire.available()) return false;
    buffer[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
  if (!g_i2cReady) return false;
  Wire.beginTransmission(address);
  const bool queued = Wire.write(reg) == 1 && Wire.write(value) == 1;
  const uint8_t status = Wire.endTransmission();
  return queued && status == 0;
}

int16_t signedLittleEndian(const uint8_t *bytes) {
  return static_cast<int16_t>(static_cast<uint16_t>(bytes[0]) |
                              (static_cast<uint16_t>(bytes[1]) << 8));
}

// Panel register sequence reproduced from Waveshare's bundled
// Arduino/examples/01_gfx_helloworld/01_gfx_helloworld.ino.
// This board's JD9853 panel needs this sequence after Arduino_ST7789::begin().
void initPanelRegisters() {
  static const uint8_t operations[] = {
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x11,
    END_WRITE,
    DELAY, 120,
    BEGIN_WRITE,
    WRITE_C8_D16, 0xDF, 0x98, 0x53,
    WRITE_C8_D8, 0xB2, 0x23,
    WRITE_COMMAND_8, 0xB7,
    WRITE_BYTES, 4, 0x00, 0x47, 0x00, 0x6F,
    WRITE_COMMAND_8, 0xBB,
    WRITE_BYTES, 6, 0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,
    WRITE_C8_D16, 0xC0, 0x44, 0xA4,
    WRITE_C8_D8, 0xC1, 0x16,
    WRITE_COMMAND_8, 0xC3,
    WRITE_BYTES, 8, 0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,
    WRITE_COMMAND_8, 0xC4,
    WRITE_BYTES, 12,
    0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,
    WRITE_COMMAND_8, 0xC8,
    WRITE_BYTES, 32,
    0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
    0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
    0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
    0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
    WRITE_COMMAND_8, 0xD0,
    WRITE_BYTES, 5, 0x04, 0x06, 0x6B, 0x0F, 0x00,
    WRITE_C8_D16, 0xD7, 0x00, 0x30,
    WRITE_C8_D8, 0xE6, 0x14,
    WRITE_C8_D8, 0xDE, 0x01,
    WRITE_COMMAND_8, 0xB7,
    WRITE_BYTES, 5, 0x03, 0x13, 0xEF, 0x35, 0x35,
    WRITE_COMMAND_8, 0xC1,
    WRITE_BYTES, 3, 0x14, 0x15, 0xC0,
    WRITE_C8_D16, 0xC2, 0x06, 0x3A,
    WRITE_C8_D16, 0xC4, 0x72, 0x12,
    WRITE_C8_D8, 0xBE, 0x00,
    WRITE_C8_D8, 0xDE, 0x02,
    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3, 0x00, 0x02, 0x00,
    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3, 0x01, 0x02, 0x00,
    WRITE_C8_D8, 0xDE, 0x00,
    WRITE_C8_D8, 0x35, 0x00,
    WRITE_C8_D8, 0x3A, 0x05,
    WRITE_COMMAND_8, 0x2A,
    WRITE_BYTES, 4, 0x00, 0x22, 0x00, 0xCD,
    WRITE_COMMAND_8, 0x2B,
    WRITE_BYTES, 4, 0x00, 0x00, 0x01, 0x3F,
    WRITE_C8_D8, 0xDE, 0x02,
    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 3, 0x00, 0x02, 0x00,
    WRITE_C8_D8, 0xDE, 0x00,
    WRITE_C8_D8, 0x36, 0x00,
    WRITE_COMMAND_8, 0x21,
    END_WRITE,
    DELAY, 10,
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29,
    END_WRITE
  };
  g_displayBus.batchOperation(operations, sizeof(operations));
}

TouchSample touchReadFailed(uint32_t now) {
  // A brief bus fault should not break a swipe; a persistent one must release.
  if (static_cast<uint32_t>(now - g_lastTouchSuccess) >=
      kTouchErrorReleaseMs) g_touch.down = false;
  return g_touch;
}

}  // namespace

bool beginDisplay() {
  if (g_displayReady) return true;
  pinMode(kLcdBacklight, OUTPUT);
  digitalWrite(kLcdBacklight, LOW);
  pinMode(kLcdCs, OUTPUT);
  pinMode(kSdCs, OUTPUT);
  digitalWrite(kLcdCs, HIGH);
  digitalWrite(kSdCs, HIGH);
  SPI.begin(kSpiSck, kSpiMiso, kSpiMosi);
  if (!g_display.begin(kLcdFrequency)) return false;
  initPanelRegisters();
  g_display.setRotation(0);
  g_display.fillScreen(RGB565_BLACK);
  g_backlightReady = ledcAttach(kLcdBacklight, kBacklightFrequency,
                                kBacklightResolution);
  if (!g_backlightReady) {
    digitalWrite(kLcdBacklight, HIGH);
    Serial.println("Backlight PWM initialization failed.");
    return false;
  }
  g_displayReady = true;
  setBrightness(80);
  return true;
}

Arduino_GFX *display() {
  return &g_display;
}

void setBrightness(uint8_t percent) {
  if (percent > 100) percent = 100;
  const uint32_t duty = (static_cast<uint32_t>(kBacklightMax) * percent + 50U) /
                        100U;
  if (g_backlightReady) ledcWrite(kLcdBacklight, duty);
}

bool beginI2C() {
  if (g_i2cReady) return true;
  g_i2cReady = Wire.begin(kI2cSda, kI2cScl, 400000);
  if (g_i2cReady) Wire.setTimeOut(25);
  return g_i2cReady;
}

bool beginTouch() {
  if (g_touchReady) return true;
  if (!beginI2C()) return false;
  pinMode(kTouchReset, OUTPUT);
  pinMode(kTouchInterrupt, INPUT_PULLUP);
  digitalWrite(kTouchReset, LOW);
  delay(200);
  digitalWrite(kTouchReset, HIGH);
  delay(300);
  uint8_t id[3];
  if (!readRegisters(kTouchAddress, 0x08, id, sizeof(id))) return false;
  g_touch = {false, 0, 0};
  g_touchIrq = false;
  g_lastTouchPoll = millis() - kTouchPollMs;
  g_lastTouchSuccess = millis();
  attachInterrupt(digitalPinToInterrupt(kTouchInterrupt), touchInterrupt,
                  FALLING);
  g_touchReady = true;
  return true;
}

TouchSample readTouch() {
  if (!g_touchReady) return {false, 0, 0};
  const uint32_t now = millis();
  noInterrupts();
  const bool interrupted = g_touchIrq;
  g_touchIrq = false;
  interrupts();
  // Poll even without another edge, so holding a finger remains a press.
  if (!interrupted &&
      static_cast<uint32_t>(now - g_lastTouchPoll) < kTouchPollMs) return g_touch;
  g_lastTouchPoll = now;
  uint8_t packet[14] = {};
  if (!readRegisters(kTouchAddress, 0x01, packet, sizeof(packet)))
    return touchReadFailed(now);
  const uint8_t count = packet[1];
  if (count > 2) return touchReadFailed(now);  // 14 bytes can hold two points.
  if (count == 0) {
    g_lastTouchSuccess = now;
    g_touch.down = false;
    return g_touch;
  }
  const uint16_t rawX = (static_cast<uint16_t>(packet[2] & 0x0F) << 8) |
                        packet[3];
  const uint16_t rawY = (static_cast<uint16_t>(packet[4] & 0x0F) << 8) |
                        packet[5];
  if (rawX >= kScreenWidth || rawY >= kScreenHeight)
    return touchReadFailed(now);
  // Official rotation 0 transform: X is mirrored relative to panel pixels.
  g_touch = {true, static_cast<uint16_t>(kScreenWidth - 1U - rawX), rawY};
  g_lastTouchSuccess = now;
  return g_touch;
}

bool beginIMU() {
  // An explicit retry must probe and reconfigure even after a prior success.
  g_imuPresent = false;
  g_imu.valid = false;
  g_imu.sampledAt = 0;
  if (!beginI2C()) return false;
  uint8_t identity = 0;
  if (!readRegisters(kImuAddress, 0x00, &identity, 1) || identity != 0x05)
    return false;
  if (!writeRegister(kImuAddress, 0x60, 0xFF)) return false;
  delay(100);
  // CTRL1 auto increment; CTRL2 +/-16g 500Hz; CTRL3 +/-2048dps 500Hz;
  // CTRL5 accel/gyro LPF, CTRL7 enable both with independent data ready flags.
  if (!writeRegister(kImuAddress, 0x02, 0x40) ||
      !writeRegister(kImuAddress, 0x03, 0x34) ||
      !writeRegister(kImuAddress, 0x04, 0x74) ||
      !writeRegister(kImuAddress, 0x06, 0x55) ||
      !writeRegister(kImuAddress, 0x08, 0x03)) return false;
  delay(100);
  uint8_t accelConfig = 0, gyroConfig = 0, enabled = 0;
  if (!readRegisters(kImuAddress, 0x03, &accelConfig, 1) || accelConfig != 0x34 ||
      !readRegisters(kImuAddress, 0x04, &gyroConfig, 1) || gyroConfig != 0x74 ||
      !readRegisters(kImuAddress, 0x08, &enabled, 1) || (enabled & 0x03) != 0x03)
    return false;
  g_imuPresent = true;
  return true;
}

ImuSample readIMU() {
  g_imu.valid = false;
  if (!g_imuPresent) return g_imu;
  uint8_t status = 0;
  if (!readRegisters(kImuAddress, 0x2E, &status, 1) || (status & 0x03) != 0x03)
    return g_imu;
  // TEMP_L/H followed immediately by AX/AY/AZ and GX/GY/GZ, all little endian.
  uint8_t bytes[14];
  if (!readRegisters(kImuAddress, 0x33, bytes, sizeof(bytes))) return g_imu;
  constexpr float accelScale = 16.0f / 32768.0f;
  constexpr float gyroScale = 2048.0f / 32768.0f;
  g_imu.temp = signedLittleEndian(bytes) / 256.0f;
  g_imu.ax = signedLittleEndian(bytes + 2) * accelScale;
  g_imu.ay = signedLittleEndian(bytes + 4) * accelScale;
  g_imu.az = signedLittleEndian(bytes + 6) * accelScale;
  g_imu.gx = signedLittleEndian(bytes + 8) * gyroScale;
  g_imu.gy = signedLittleEndian(bytes + 10) * gyroScale;
  g_imu.gz = signedLittleEndian(bytes + 12) * gyroScale;
  g_imu.sampledAt = millis();
  g_imu.valid = true;
  return g_imu;
}

bool imuPresent() {
  return g_imuPresent;
}

}  // namespace BoardHW
