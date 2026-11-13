#pragma once
#ifdef COLOPLAY_USB_CONCURRENCY_TEST
#include <Arduino.h>
#include <WebServer.h>
namespace UsbNetwork {
void experimentSetup(WebServer &server);
void experimentTick();
}
#endif
