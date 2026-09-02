#ifndef COCKPIT_EVDEV_READER_H_
#define COCKPIT_EVDEV_READER_H_

#include <string>

#include "cockpit/input_source.h"
#include "cockpit/mapping.h"

struct libevdev;

namespace cockpit {

class EvdevReader : public InputSource {
 public:
  EvdevReader(std::string device_path, MappingConfig mapping, bool print_raw);
  ~EvdevReader() override;

  EvdevReader(const EvdevReader&) = delete;
  EvdevReader& operator=(const EvdevReader&) = delete;

  void Drain() override;
  const G29State& state() const override { return state_; }

  // Try open (or reopen after unplug). Returns false if still missing.
  bool OpenOrReopen();

 private:
  void Close();
  void ApplyEvent(int type, int code, int value);
  void ApplyKey(int code, int value);
  double AxisToSteeringDeg(int value) const;
  double AxisToPedal(int code, int value) const;
  void RefreshAbsInfo();

  std::string device_path_;
  MappingConfig mapping_;
  bool print_raw_ = false;
  int fd_ = -1;
  libevdev* dev_ = nullptr;
  G29State state_{};
  int rotary_pos_ = 0;
};

}  // namespace cockpit

#endif  // COCKPIT_EVDEV_READER_H_
