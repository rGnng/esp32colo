#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "scene_settings.h"
#include "action_feedback.h"
#include "gesture_policy.h"
namespace SceneService {
using Handler = int (*)(const SceneSettings::Action &, int);
using Feedback = ActionFeedback::State;
void begin(Handler handler);
String statusJson();
String configJson();
bool configure(WebServer &server, String &error);
void event(int input, int gesture, int repetitions=1);
uint32_t epoch();
uint16_t combinations();
GestureBindings gestureBindings();
uint8_t activeScene();
uint8_t icon();
const char *name();
SceneSettings::Level brightness(int channel);
int adjustBrightness(int channel,int value,bool relative);
void tick();
uint8_t screen();
Feedback feedback();
SceneSettings::Light lighting(bool ring);
}  // namespace SceneService
