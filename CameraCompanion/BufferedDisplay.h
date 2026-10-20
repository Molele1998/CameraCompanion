#pragma once

#include <Arduino_GFX_Library.h>

// Render a complete scene into one 40-row RAM band at a time. The LCD receives
// only completed, changed bands; intermediate clear/text drawing never reaches
// the panel. This needs 13,760 bytes instead of a 110,080-byte full framebuffer.
class BufferedDisplay final : public Arduino_GFX {
public:
  static constexpr int16_t kWidth = 172;
  static constexpr int16_t kHeight = 320;
  static constexpr int16_t kBandHeight = 40;
  static constexpr int16_t kBandCount = kHeight / kBandHeight;

  BufferedDisplay();
  bool begin(int32_t speed = GFX_NOT_DEFINED) override;
  void attach(Arduino_GFX *panel);
  void beginBand(uint8_t index);
  void presentBand();
  void clearBand(uint16_t color=0);
  void setViewport(int16_t dx=0,int16_t dy=0,int16_t sourceTop=0,int16_t sourceBottom=kHeight);

  void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) override;
  void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
  void writeFillRectPreclipped(int16_t x, int16_t y, int16_t w,
                              int16_t h, uint16_t color) override;

private:
  Arduino_GFX *panel_ = nullptr;
  uint16_t pixels_[kWidth * kBandHeight] = {};
  uint32_t lastHash_[kBandCount] = {};
  bool valid_[kBandCount] = {};
  uint8_t band_ = 0;
  int16_t top_ = 0;
  int16_t dx_=0,dy_=0,sourceTop_=0,sourceBottom_=kHeight;
};
