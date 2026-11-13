#include "rotary_detents.h"
#include <cassert>
#include <iostream>

int main() {
  RotaryDetents knob;
  // A full electrical cycle spans two mechanical notches on this encoder.
  // Check one-edge polling in both directions, including a reversal at zero.
  for (int direction : {1, -1, -1, 1}) {
    for (int notch = 0; notch < 20; ++notch) {
      assert(knob.consume(direction) == 0);
      assert(knob.consume(direction) == direction);
    }
  }
  // Partial turns and contact bounce must cancel without UI movement.
  for (int direction : {1, -1}) {
    assert(knob.consume(direction) == 0);
    assert(knob.consume(-direction) == 0);
    assert(knob.consume(2 * direction) == direction);
    assert(knob.consume(-direction) == 0);
    assert(knob.consume(direction) == 0);
    assert(knob.consume(-2 * direction) == -direction);
  }
  // PCNT may batch several detents between polls; retain the odd edge.
  assert(knob.consume(7) == 3);
  assert(knob.consume(1) == 1);
  assert(knob.consume(-5) == -2);
  assert(knob.consume(1) == 0);
  assert(knob.consume(-2) == -1);
  std::cout << "PASS rotary notches, reversals, bounce and batched edges\n";
}
