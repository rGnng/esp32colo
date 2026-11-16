#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "navigation_feedback.h"
namespace BluetoothService {
struct Snapshot {
  NavigationFeedback::State navigation;
  bool navigationSupported=false;
  bool floatVolume=false,enabled=false,ready=false,connected=false,secured=false,extension=false,fresh=false,available=false,pending=false,positionValid=false;
  int playback=255,seek=0,result=0;
  float volume=-1.0f;
  bool volumePrediction=false,predictionReady=false,predicting=false;
  int volumeSteps=0;
  unsigned volumePending=0;
  uint16_t volumeCompleted=0;
  float gridTarget=-1.0f,preview=-1.0f,displayVolume=-1.0f;
  uint16_t request=0,usage=0;
  uint32_t elapsedMs=0,durationMs=0,receivedAt=0,positionMs=0;
  uint64_t track=0;
  char title[64]={},artist[64]={};
};
void load();
void tick(bool hardwareReady);
bool configure(WebServer &,String &error);
String statusJson();
Snapshot snapshot();
bool send(uint8_t kind,uint16_t usage,int count,uint16_t parameter=0,uint16_t duration=0);
void cancel();
}
