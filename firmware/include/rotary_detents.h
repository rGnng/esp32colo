#pragma once
#include <cstdint>

// This encoder has two detents per complete quadrature cycle. PCNT counts
// all four edges, so each physical notch contributes two signed edges.
class RotaryDetents {
 public:
  static constexpr int kEdgesPerDetent = 2;

  int32_t consume(int32_t edges) {
    const int32_t total = pending_ + edges;
    const int32_t steps = total / kEdgesPerDetent;
    pending_ = total % kEdgesPerDetent;
    return steps;
  }

 private:
  // Retain partial movement relative to the last reported detent, so a
  // single reverse edge (including bounce) does not undo a complete step.
  int32_t pending_ = 0;
};
