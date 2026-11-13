#pragma once
#include <Arduino.h>
#include <WebServer.h>
namespace GeneralService {
void begin();
bool sceneChangeSound();
uint32_t sleepTimeoutMinutes();
String statusJson();
bool configure(WebServer &server,String &error);
}
