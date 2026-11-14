#pragma once
#include <Arduino.h>
#include <WebServer.h>
namespace InputTrace {
void setup(WebServer &server);
void begin(bool raw,bool down);
void sample(bool raw);
void debounced(bool down);
void gesture(int input,int gesture);
}
