#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "playback_settings.h"
namespace PlaybackService {
void begin(const PlaybackSettings::MigrationContext &context);
const PlaybackSettings::Stored &settings();
String statusJson();
bool configure(WebServer &server,String &error);
}
