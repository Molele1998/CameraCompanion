#include <Arduino.h>
#if !defined(CONFIG_IDF_TARGET_ESP32C6)
#error "Select ESP32C6 Dev Module."
#endif
#if !defined(ARDUINO_USB_CDC_ON_BOOT) || !ARDUINO_USB_CDC_ON_BOOT
#error "Enable USB CDC On Boot."
#endif
#include "CameraUi.h"
void setup(){CameraUi::begin();}
void loop(){CameraUi::loop();}
