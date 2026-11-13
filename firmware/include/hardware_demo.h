#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "scene_settings.h"

namespace HardwareDemo {
void captureBootStage();
void begin();
void tick();
void pause(bool paused);
bool ready();
String statusJson();
String buttonsJson();
// Form fields, ranges and persistence are documented in docs/api.md.
// `auto` controls page touring; brightness Auto flags are independent.
bool configure(WebServer &server, String &error);
int applyAction(const SceneSettings::Action &action, int repetitions);
}  // namespace HardwareDemo
