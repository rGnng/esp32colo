#pragma once
#include <Arduino.h>
#include <WebServer.h>

namespace HardwareDemo {
void captureBootStage();
void begin();
void tick();
void pause(bool paused);
bool ready();
String statusJson();
String buttonsJson();
// Arguments: page (0..3), auto (0/1), brightness (0..100), leds (0..64),
// volume (0..100), tone (100..4000 Hz), duration (30..1500 ms), awake (0/1),
// led_test (0=rainbow, 1=white, 2=red, 3=green, 4=blue; returns after 5 s).
bool configure(WebServer &server, String &error);
void sendScreenshot(WebServer &server);
}  // namespace HardwareDemo
