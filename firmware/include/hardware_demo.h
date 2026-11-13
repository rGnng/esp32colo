#pragma once
#include <Arduino.h>
#include <WebServer.h>

namespace HardwareDemo {
void begin();
void tick();
void pause(bool paused);
bool ready();
String statusJson();
String buttonsJson();
// Arguments: page (0..3), auto (0/1), brightness (0..100), leds (0..64),
// volume (0..85), tone (100..4000 Hz), duration (30..1500 ms), awake (0/1).
bool configure(WebServer &server, String &error);
void sendScreenshot(WebServer &server);
}  // namespace HardwareDemo
