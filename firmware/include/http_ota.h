#pragma once
#include <WebServer.h>

namespace HttpOta {
void setup(WebServer &server, const char *password);
bool busy();
void tick();
}  // namespace HttpOta
