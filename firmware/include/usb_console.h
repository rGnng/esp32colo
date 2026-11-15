#pragma once
#include <Arduino.h>
namespace UsbConsole {
Stream &stream();
bool prepareNetwork();
void useNetwork(bool enabled);
void tick();
bool connected();
uint32_t dropped();
size_t queued();
}
