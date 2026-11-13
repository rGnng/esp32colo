#pragma once
#include <Arduino.h>
#include <WebServer.h>

namespace UsbNetwork {
void prepareSerial();
void loadSettings();
void begin(bool recoveryMode);
String statusJson();
bool configure(WebServer &server, String &error);
}  // namespace UsbNetwork
