#pragma once
#include "CameraModel.h"
namespace Gpx {
enum class Command:uint8_t { Check,Start,Stop };
struct Snapshot {bool available=false,mounted=false,recording=false,busy=false;uint32_t points=0;int error=0;char file[20]{};};
bool begin();
bool request(Command command);
void offer(const Camera::Fix& fix,bool available); // Current firmware: explicit SIM source only.
Snapshot snapshot();
bool lockDisplay(); // Non-blocking, same SPI owner as the SD worker.
void unlockDisplay();
}
