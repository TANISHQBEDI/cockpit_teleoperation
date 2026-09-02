#ifndef COCKPIT_MOCK_READER_H_
#define COCKPIT_MOCK_READER_H_

#include <cstdint>

#include "cockpit/input_source.h"
#include "cockpit/mapping.h"

namespace cockpit {

// Synthetic G29 for Mac Docker / CI. Sweeps steering at ~0.25 Hz so JSON
// is visibly changing at 500 Hz without USB HID.
class MockReader : public InputSource {
 public:
  explicit MockReader(MappingConfig mapping);

  void Drain() override;
  const G29State& state() const override { return state_; }

 private:
  MappingConfig mapping_;
  G29State state_{};
  std::uint64_t ticks_ = 0;
};

}  // namespace cockpit

#endif  // COCKPIT_MOCK_READER_H_
