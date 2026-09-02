#include "cockpit/evdev_reader.h"

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>

#include <libevdev/libevdev.h>

namespace cockpit {
namespace {

bool KeyDown(int value) { return value != 0; }

}  // namespace

EvdevReader::EvdevReader(std::string device_path, MappingConfig mapping,
                         bool print_raw)
    : device_path_(std::move(device_path)),
      mapping_(std::move(mapping)),
      print_raw_(print_raw) {
  state_.block_reason = "DISCONNECTED";
  OpenOrReopen();
}

EvdevReader::~EvdevReader() { Close(); }

void EvdevReader::Close() {
  if (dev_ != nullptr) {
    libevdev_free(dev_);
    dev_ = nullptr;
  }
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }
  state_.connected = false;
  state_.block_reason = "DISCONNECTED";
}

bool EvdevReader::OpenOrReopen() {
  if (dev_ != nullptr) {
    return true;
  }
  Close();

  fd_ = open(device_path_.c_str(), O_RDONLY | O_NONBLOCK);
  if (fd_ < 0) {
    return false;
  }
  const int rc = libevdev_new_from_fd(fd_, &dev_);
  if (rc < 0) {
    std::cerr << "[g29_reader] libevdev_new_from_fd(" << device_path_
              << "): " << strerror(-rc) << "\n";
    Close();
    return false;
  }

  std::cerr << "[g29_reader] opened " << device_path_ << " name=\""
            << libevdev_get_name(dev_) << "\" vid=" << std::hex
            << libevdev_get_id_vendor(dev_) << " pid="
            << libevdev_get_id_product(dev_) << std::dec << "\n";

  const int vid = libevdev_get_id_vendor(dev_);
  const int pid = libevdev_get_id_product(dev_);
  if (vid == 0x046d && pid == 0xc260) {
    std::cerr << "[g29_reader] G29 is in PS4 mode. Switch to PS3/PC.\n";
    Close();
    state_.block_reason = "DISCONNECTED";
    return false;
  }

  RefreshAbsInfo();
  state_.connected = true;
  state_.block_reason.clear();
  return true;
}

void EvdevReader::RefreshAbsInfo() {
  // Seed axes from current kernel values so the first 500 Hz frame is not zero.
  auto seed = [this](int code) {
    if (dev_ == nullptr || !libevdev_has_event_code(dev_, EV_ABS, code)) {
      return;
    }
    const int value = libevdev_get_event_value(dev_, EV_ABS, code);
    ApplyEvent(EV_ABS, code, value);
  };
  seed(mapping_.steer_abs_code);
  seed(mapping_.accel_abs_code);
  seed(mapping_.brake_abs_code);
  seed(mapping_.clutch_abs_code);
  seed(mapping_.hat_x_abs_code);
  seed(mapping_.hat_y_abs_code);
}

void EvdevReader::Drain() {
  if (dev_ == nullptr) {
    OpenOrReopen();
    return;
  }

  struct input_event ev {};
  unsigned int flags = LIBEVDEV_READ_FLAG_NORMAL;
  int rc = 0;
  do {
    rc = libevdev_next_event(dev_, flags, &ev);
    if (rc == LIBEVDEV_READ_STATUS_SYNC) {
      flags = LIBEVDEV_READ_FLAG_SYNC;
      continue;
    }
    if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
      flags = LIBEVDEV_READ_FLAG_NORMAL;
      ApplyEvent(ev.type, ev.code, ev.value);
    }
  } while (rc == LIBEVDEV_READ_STATUS_SUCCESS ||
           rc == LIBEVDEV_READ_STATUS_SYNC);

  if (rc == -ENODEV || rc == -ENOENT) {
    std::cerr << "[g29_reader] device vanished, waiting to reopen\n";
    Close();
  } else if (rc < 0 && rc != -EAGAIN) {
    std::cerr << "[g29_reader] read error: " << strerror(-rc) << "\n";
  }
}

void EvdevReader::ApplyEvent(int type, int code, int value) {
  if (print_raw_ && (type == EV_KEY || type == EV_ABS)) {
    const char* tname = libevdev_event_type_get_name(type);
    const char* cname = libevdev_event_code_get_name(type, code);
    std::cerr << "[raw] type=" << type << " ("
              << (tname != nullptr ? tname : "?") << ") code=" << code << " ("
              << (cname != nullptr ? cname : "?") << ") value=" << value
              << "\n";
  }

  if (type == EV_KEY) {
    ApplyKey(code, value);
    return;
  }
  if (type != EV_ABS) {
    return;
  }

  if (code == mapping_.steer_abs_code) {
    state_.steering_angle_deg = AxisToSteeringDeg(value);
  } else if (code == mapping_.accel_abs_code) {
    state_.accelerator_pedal = AxisToPedal(code, value);
  } else if (code == mapping_.brake_abs_code) {
    state_.brake_pedal = AxisToPedal(code, value);
  } else if (code == mapping_.clutch_abs_code) {
    state_.clutch_pedal = AxisToPedal(code, value);
  } else if (code == mapping_.hat_x_abs_code) {
    state_.dpad_left = value < 0;
    state_.dpad_right = value > 0;
  } else if (code == mapping_.hat_y_abs_code) {
    // Linux hats: -1 is up, +1 is down on most wheels.
    state_.dpad_up = value < 0;
    state_.dpad_down = value > 0;
  }
}

void EvdevReader::ApplyKey(int code, int value) {
  const bool down = KeyDown(value);
  if (code == mapping_.btn_cross) {
    state_.cross_x = down;
  } else if (code == mapping_.btn_square) {
    state_.square = down;
  } else if (code == mapping_.btn_circle) {
    state_.circle_o = down;
  } else if (code == mapping_.btn_triangle) {
    state_.triangle = down;
  } else if (code == mapping_.btn_right_paddle) {
    state_.right_paddle_upshift = down;
  } else if (code == mapping_.btn_left_paddle) {
    state_.left_paddle_downshift = down;
  } else if (code == mapping_.btn_r2) {
    state_.r2_button = down;
  } else if (code == mapping_.btn_l2) {
    state_.l2_button = down;
  } else if (code == mapping_.btn_share) {
    state_.share_button = down;
  } else if (code == mapping_.btn_options) {
    state_.option_button = down;
  } else if (code == mapping_.btn_r3) {
    state_.r3_button = down;
  } else if (code == mapping_.btn_l3) {
    state_.l3_button = down;
  } else if (code == mapping_.btn_ps) {
    state_.ps_button = down;
  } else if (code == mapping_.btn_plus) {
    const bool was = state_.plus_button;
    state_.plus_button = down;
    if (down && !was) {
      ++rotary_pos_;
      state_.rotary_dial_position = rotary_pos_;
    }
  } else if (code == mapping_.btn_minus) {
    const bool was = state_.minus_button;
    state_.minus_button = down;
    if (down && !was) {
      --rotary_pos_;
      state_.rotary_dial_position = rotary_pos_;
    }
  } else if (code == mapping_.btn_rotary_enter) {
    state_.rotary_enter_button = down;
  }
}

double EvdevReader::AxisToSteeringDeg(int value) const {
  if (dev_ == nullptr) {
    return 0.0;
  }
  const auto* info = libevdev_get_abs_info(dev_, mapping_.steer_abs_code);
  if (info == nullptr || info->maximum == info->minimum) {
    return 0.0;
  }
  const double span = static_cast<double>(info->maximum - info->minimum);
  const double mid = (info->minimum + info->maximum) / 2.0;
  const double norm = (static_cast<double>(value) - mid) / (span / 2.0);
  const double clamped = std::clamp(norm, -1.0, 1.0);
  return clamped * (mapping_.steering_range_deg / 2.0);
}

double EvdevReader::AxisToPedal(int code, int value) const {
  if (dev_ == nullptr) {
    return 0.0;
  }
  const auto* info = libevdev_get_abs_info(dev_, code);
  if (info == nullptr || info->maximum == info->minimum) {
    return 0.0;
  }
  const double span = static_cast<double>(info->maximum - info->minimum);
  double n = (static_cast<double>(value) - info->minimum) / span;
  n = std::clamp(n, 0.0, 1.0);
  if (mapping_.invert_pedals) {
    n = 1.0 - n;
  }
  return n;
}

}  // namespace cockpit
