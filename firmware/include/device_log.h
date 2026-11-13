#pragma once
#include <Arduino.h>
#include <WebServer.h>
namespace DeviceLog {
void printf(const char *format,...) __attribute__((format(printf,1,2)));
void println(const char *message="");
void println(const String &message);
void setup(WebServer &server);
}
