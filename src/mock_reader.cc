#include "cockpit/mock_reader.h"

#include <cmath>

namespace cockpit {

MockReader::MockReader(MappingConfig mapping) : mapping_(std::move(mapping)) {
  state_.connected = true;
  state_.block_reason.clear();
}

void MockReader::Drain() {
  ++ticks_;
  // ~0.25 Hz sine across ±(range/2). At 500 Hz this is 2000 ticks/cycle.
  constexpr double kTwoPi = 6.283185307179586;
  const double phase = static_cast<double>(ticks_) / 2000.0 * kTwoPi;
  state_.steering_angle_deg =
      std::sin(phase) * (mapping_.steering_range_deg / 2.0);
  state_.accelerator_pedal = 0.5 + 0.5 * std::sin(phase * 0.5);
  state_.brake_pedal = 0.0;
  state_.clutch_pedal = 0.0;
  state_.rotary_dial_position = static_cast<int>(ticks_ / 500) % 8;
}

}  // namespace cockpit
