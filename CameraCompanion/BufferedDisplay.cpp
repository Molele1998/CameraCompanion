#include "BufferedDisplay.h"

#include <algorithm>

BufferedDisplay::BufferedDisplay() : Arduino_GFX(kWidth, kHeight) {}

bool BufferedDisplay::begin(int32_t) {
  // BoardHW owns panel initialization and the panel-specific register sequence.
  return panel_ != nullptr;
}

void BufferedDisplay::attach(Arduino_GFX *panel) {
  panel_ = panel;
  std::fill_n(valid_, kBandCount, false);
}

void BufferedDisplay::beginBand(uint8_t index) {
  band_ = index < kBandCount ? index : kBandCount - 1;
  top_ = band_ * kBandHeight;
  setViewport();
}
void BufferedDisplay::clearBand(uint16_t color){std::fill_n(pixels_,kWidth*kBandHeight,color);}
void BufferedDisplay::setViewport(int16_t dx,int16_t dy,int16_t sourceTop,int16_t sourceBottom){dx_=dx;dy_=dy;sourceTop_=sourceTop;sourceBottom_=sourceBottom;}

void BufferedDisplay::presentBand() {
  if (!panel_) return;
  uint32_t hash = 2166136261UL;
  for (uint16_t pixel : pixels_) {
    hash ^= pixel;
    hash *= 16777619UL;
  }
  if (valid_[band_] && lastHash_[band_] == hash) return;
  panel_->draw16bitRGBBitmap(0, top_, pixels_, kWidth, kBandHeight);
  lastHash_[band_] = hash;
  valid_[band_] = true;
}

void BufferedDisplay::writePixelPreclipped(int16_t x, int16_t y, uint16_t color) {
  if(x<0 || x>=kWidth || y<sourceTop_ || y>=sourceBottom_)return;
  x+=dx_;y+=dy_;
  if(y<sourceTop_ || y>=sourceBottom_)return;
  if (x < 0 || x >= kWidth || y < top_ || y >= top_ + kBandHeight) return;
  pixels_[(y - top_) * kWidth + x] = color;
}

void BufferedDisplay::writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if(y<sourceTop_ || y>=sourceBottom_)return;
  const int sourceLeft=std::max<int>(0,x),sourceRight=std::min<int>(kWidth,int(x)+w);
  x=sourceLeft+dx_;w=sourceRight-sourceLeft;y+=dy_;
  if(y<sourceTop_ || y>=sourceBottom_)return;
  if (y < top_ || y >= top_ + kBandHeight || w <= 0) return;
  const int left = std::max<int>(0, x);
  const int right = std::min<int>(kWidth, static_cast<int>(x) + w);
  if (right <= left) return;
  std::fill_n(pixels_ + (y - top_) * kWidth + left, right - left, color);
}

void BufferedDisplay::writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if(x<0 || x>=kWidth)return;
  const int sourceFirst=std::max<int>(sourceTop_,y),sourceLast=std::min<int>(sourceBottom_,int(y)+h);
  x+=dx_;y=sourceFirst+dy_;h=sourceLast-sourceFirst;
  if (x < 0 || x >= kWidth || h <= 0) return;
  const int first = std::max<int>(std::max<int>(top_,sourceTop_), y);
  const int last = std::min<int>(std::min<int>(top_ + kBandHeight,sourceBottom_), static_cast<int>(y) + h);
  for (int row = first; row < last; ++row) pixels_[(row - top_) * kWidth + x] = color;
}

void BufferedDisplay::writeFillRectPreclipped(int16_t x, int16_t y, int16_t w,
                                             int16_t h, uint16_t color) {
  if (w <= 0 || h <= 0) return;
  const int sourceLeft=std::max<int>(0,x),sourceRight=std::min<int>(kWidth,int(x)+w);
  const int sourceFirst=std::max<int>(sourceTop_,y),sourceLast=std::min<int>(sourceBottom_,int(y)+h);
  x=sourceLeft+dx_;y=sourceFirst+dy_;w=sourceRight-sourceLeft;h=sourceLast-sourceFirst;
  const int left = std::max<int>(0, x);
  const int right = std::min<int>(kWidth, static_cast<int>(x) + w);
  const int first = std::max<int>(std::max<int>(top_,sourceTop_), y);
  const int last = std::min<int>(std::min<int>(top_ + kBandHeight,sourceBottom_), static_cast<int>(y) + h);
  if (right <= left) return;
  for (int row = first; row < last; ++row) {
    std::fill_n(pixels_ + (row - top_) * kWidth + left, right - left, color);
  }
}
