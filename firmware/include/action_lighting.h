#pragma once
#include "action_feedback.h"
#include "demo_levels.h"
#include "lighting_settings.h"
#include "ring_fill.h"

namespace ActionLighting {
struct Result {uint32_t color;int brightness;bool applied;};
// Colors are gamma-corrected by the renderer before brightness is applied.
inline Result resolve(const ActionFeedback::Light &cue,uint32_t sceneColor,
                      uint32_t effectColor,uint32_t rainbowColor,int sceneBrightness,
                      bool ring,int pixel,int ringPixels,bool remoteFresh,float remotePercent,
                      uint8_t ringStyle=RingFill::kClockwise,int topPixel=0){
  using namespace SceneSettings;
  Result out{sceneColor,sceneBrightness,false};
  if(cue.led==kInherit)return out;
  if(cue.led==kProportional&&cue.remote&&(!remoteFresh||!(remotePercent>=0&&remotePercent<=100)))return out;
  out.applied=true;if(cue.brightness>=0)out.brightness=cue.brightness;
  if(cue.led==kOff)out.color=0;
  else if(cue.led==kFeedbackStatic)out.color=effectColor;
  else if(cue.led==kFeedbackRainbow)out.color=rainbowColor;
  else if(cue.led==kProportional){
    const float percent=cue.remote?remotePercent:cue.percent;
    const int permille=percent<0?0:percent>100?1000:int(percent*10+0.5f);
    const int coverage=ring?RingFill::coverage(permille,pixel,ringPixels,ringStyle,topPixel):permille;
    out.color=DemoLevels::scaleRgb(effectColor,(coverage*255+500)/1000);
  }
  return out;
}
}
