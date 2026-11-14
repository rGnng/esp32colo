#pragma once
#include <Arduino.h>
#include <WebServer.h>
namespace GeneralService {
void begin();
bool sceneChangeSound();
uint32_t sleepTimeoutMinutes();
uint32_t soundVolume();
String statusJson();
bool configure(WebServer &server,String &error);
}
