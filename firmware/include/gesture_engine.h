#pragma once
#include "scene_settings.h"
#include <cstdint>

// Nonblocking independent gesture recognizers, with rollover-safe durations.
class GestureEngine {
 public:
  static constexpr uint32_t kDebounceMs = 25, kClickMs = 300, kHoldMs = 600;
  struct State {
    bool raw = false, down = false, blocked = false, held = false, turned = false;
    uint8_t clicks = 0;
    uint32_t rawAt = 0, downAt = 0, upAt = 0;
  };
  void seed(const bool *raw, uint32_t now) {
    for (int i=0;i<SceneSettings::kInputCount;++i) {
      states_[i] = State{}; states_[i].raw = states_[i].down = raw[i];
      states_[i].blocked = raw[i]; states_[i].rawAt = now;
    }
  }
  void cancel() {
    for (auto &s : states_) { s.clicks = 0; s.blocked = s.down || s.raw; s.held = s.turned = false; }
  }
  void suppress(uint8_t mask) {
    for(int i=0;i<4;++i)if(mask&(1u<<i)) {
      auto &s=states_[i];s.clicks=0;s.blocked=s.raw||s.down;s.held=s.turned=false;
    }
  }
  bool down(int i) const { return states_[i].down; }
  template <typename Emit> void tick(const bool *raw, int32_t detents, uint32_t now, Emit emit) {
    using namespace SceneSettings;
    for (int i=0;i<kInputCount;++i) {
      auto &s = states_[i];
      if (raw[i] != s.raw) { s.raw = raw[i]; s.rawAt = now; }
      if (s.raw == s.down || uint32_t(now-s.rawAt) < kDebounceMs) continue;
      s.down = s.raw;
      if (s.down) {
        // A new press arriving beyond the click window belongs to a new sequence.
        if (s.clicks && uint32_t(now-s.upAt) >= kClickMs) { emit(i,s.clicks-1,1); s.clicks=0; }
        s.downAt = now; s.held = false;
        if (i != kKnob) s.turned = false;
      } else {
        if (!s.blocked && !s.turned) {
          if (!s.held && uint32_t(now-s.downAt)>=kHoldMs) {
            s.held=true; s.clicks=0; emit(i,kHold,1);
          }
          if (s.held) emit(i,kHoldRelease,1);
          else { ++s.clicks; s.upAt = now; if (s.clicks == 3) { emit(i,kTriple,1); s.clicks=0; } }
        }
        s.blocked = false; s.held = s.turned = false;
      }
    }
    auto &knob = states_[kKnob];
    if (detents && !knob.blocked) {
      const bool modified = knob.down || raw[kKnob];
      if (modified) { knob.turned = true; knob.clicks = 0; }
      emit(kKnob, modified ? (detents<0?kPressLeft:kPressRight) : (detents<0?kLeft:kRight), detents<0?-detents:detents);
    }
    for (int i=0;i<kInputCount;++i) {
      auto &s = states_[i];
      if (s.blocked) continue;
      if (s.down && !s.held && !s.turned && uint32_t(now-s.downAt)>=kHoldMs) {
        s.clicks=0; s.held=true; emit(i,kHold,1);
      }
      if (!s.down && s.clicks && uint32_t(now-s.upAt)>=kClickMs) {
        emit(i,s.clicks-1,1); s.clicks=0;
      }
    }
  }
 private:
  State states_[SceneSettings::kInputCount];
};
