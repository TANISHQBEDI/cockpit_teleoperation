#ifndef COCKPIT_INPUT_SOURCE_H_
#define COCKPIT_INPUT_SOURCE_H_

#include "cockpit/g29_state.h"

namespace cockpit {

class InputSource {
 public:
  virtual ~InputSource() = default;

  // Non-blocking: apply pending hardware (or mock) events into state.
  virtual void Drain() = 0;

  virtual const G29State& state() const = 0;
};

}  // namespace cockpit

#endif  // COCKPIT_INPUT_SOURCE_H_
