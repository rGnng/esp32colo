#pragma once
namespace SceneCue {
constexpr unsigned kPreviousHz=440,kNextHz=880,kDurationMs=90;
// Explicit direction preserves previous/next at wraparound. Direct selection
// follows scene ID order. No scene change means no cue.
inline unsigned frequency(bool enabled,int previous,int next,int direction=0){
  if(!enabled||previous==next)return 0;
  if(!direction)direction=next>previous?1:-1;
  return direction>0?kNextHz:kPreviousHz;
}
}
