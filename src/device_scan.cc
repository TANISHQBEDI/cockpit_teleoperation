#include "cockpit/device_scan.h"

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <libevdev/libevdev.h>

namespace cockpit {
namespace {

std::string ToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

std::string EventPathFromHandlers(const std::string& handlers) {
  std::istringstream iss(handlers);
  std::string tok;
  while (iss >> tok) {
    if (tok.rfind("event", 0) == 0) {
      return "/dev/input/" + tok;
    }
  }
  return {};
}

void PrintPermissionHint() {
  std::cerr << "[g29_reader] input nodes are group 'input' (mode 0660). "
               "This user cannot open /dev/input/event*.\n"
            << "  today:    sudo ./build/g29_reader --list-devices\n"
            << "            sudo docker compose run --rm "
               "--device /dev/input:/dev/input g29-reader --list-devices\n"
            << "  forever:  sudo usermod -aG input \"$USER\"\n"
            << "            then exit SSH and log in again (newgrp is not enough "
               "for every session).\n";
}

bool CanOpenEventNode(const std::string& path) {
  if (path.empty()) {
    return false;
  }
  const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    return false;
  }
  close(fd);
  return true;
}

}  // namespace

std::vector<InputDeviceInfo> ScanInputDevices() {
  std::vector<InputDeviceInfo> out;
  std::ifstream in("/proc/bus/input/devices");
  if (!in) {
    return out;
  }

  InputDeviceInfo cur;
  std::string line;
  auto flush = [&]() {
    if (!cur.event_path.empty()) {
      out.push_back(cur);
    }
    cur = {};
  };

  while (std::getline(in, line)) {
    if (line.empty()) {
      flush();
      continue;
    }
    if (line.rfind("I:", 0) == 0) {
      std::istringstream iss(line.substr(2));
      std::string tok;
      while (iss >> tok) {
        const auto eq = tok.find('=');
        if (eq == std::string::npos) {
          continue;
        }
        const std::string k = tok.substr(0, eq);
        const std::string v = ToLower(tok.substr(eq + 1));
        if (k == "Vendor") {
          cur.vendor = v;
        } else if (k == "Product") {
          cur.product = v;
        }
      }
    } else if (line.rfind("N: Name=", 0) == 0) {
      cur.name = line.substr(8);
      if (!cur.name.empty() && cur.name.front() == '"') {
        cur.name.erase(cur.name.begin());
        if (!cur.name.empty() && cur.name.back() == '"') {
          cur.name.pop_back();
        }
      }
    } else if (line.rfind("H: Handlers=", 0) == 0) {
      cur.event_path = EventPathFromHandlers(line.substr(12));
    }
  }
  flush();
  return out;
}

std::string FindG29EventPath() {
  namespace fs = std::filesystem;
  const fs::path by_id{"/dev/input/by-id"};
  try {
    if (fs::exists(by_id)) {
      for (const auto& entry : fs::directory_iterator(by_id)) {
        const std::string name = entry.path().filename().string();
        const std::string lower = ToLower(name);
        if (lower.find("g29") != std::string::npos &&
            lower.find("event-joystick") != std::string::npos) {
          return fs::canonical(entry.path()).string();
        }
        if (lower.find("driving_force_racing_wheel") != std::string::npos &&
            lower.find("event-joystick") != std::string::npos) {
          return fs::canonical(entry.path()).string();
        }
      }
    }
  } catch (const fs::filesystem_error& e) {
    std::cerr << "[g29_reader] cannot scan /dev/input/by-id: " << e.what()
              << "\n";
    PrintPermissionHint();
  }

  bool saw_ps4 = false;
  for (const auto& dev : ScanInputDevices()) {
    if (dev.vendor == "046d" && dev.product == "c260") {
      saw_ps4 = true;
    }
    if (dev.vendor == "046d" && dev.product == "c24f" && !dev.event_path.empty()) {
      return dev.event_path;
    }
  }
  if (saw_ps4) {
    std::cerr << "[g29_reader] found G29 in PS4 mode (046d:c260). "
                 "Flip the hardware switch to PS3 / PC mode (046d:c24f).\n";
  }
  return {};
}

void PrintDeviceList() {
  std::cerr << "[g29_reader] input devices:\n";
  if (access("/proc/bus/input/devices", R_OK) != 0) {
    std::cerr << "  cannot read /proc/bus/input/devices: " << strerror(errno)
              << "\n";
  }
  if (access("/dev/input", R_OK | X_OK) != 0) {
    std::cerr << "  cannot list /dev/input: " << strerror(errno) << "\n";
    PrintPermissionHint();
  }

  const auto devices = ScanInputDevices();
  if (devices.empty()) {
    std::cerr << "  (none parsed from /proc/bus/input/devices)\n"
              << "  host check: grep -A8 -i g29 /proc/bus/input/devices\n";
    if (access("/dev/input", R_OK | X_OK) != 0) {
      PrintPermissionHint();
    }
    return;
  }

  bool saw_g29 = false;
  bool g29_unreadable = false;
  for (const auto& d : devices) {
    const bool ok = CanOpenEventNode(d.event_path);
    std::cerr << "  " << d.event_path << "  vendor=" << d.vendor
              << " product=" << d.product << "  " << d.name << "  "
              << (ok ? "open=ok" : "open=DENIED") << "\n";
    if (d.vendor == "046d" && (d.product == "c24f" || d.product == "c260")) {
      saw_g29 = true;
      if (!ok) {
        g29_unreadable = true;
      }
    }
  }
  if (saw_g29 && g29_unreadable) {
    PrintPermissionHint();
  }
}

int PrintDeviceCaps(const std::string& event_path) {
  const int fd = open(event_path.c_str(), O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    std::cerr << "[g29_reader] open(" << event_path << "): " << strerror(errno)
              << "\n";
    return 1;
  }

  libevdev* dev = nullptr;
  const int rc = libevdev_new_from_fd(fd, &dev);
  if (rc < 0) {
    std::cerr << "[g29_reader] libevdev: " << strerror(-rc) << "\n";
    close(fd);
    return 1;
  }

  std::cerr << "[g29_reader] caps path=" << event_path << " name=\""
            << libevdev_get_name(dev) << "\" vid=0x" << std::hex
            << libevdev_get_id_vendor(dev) << " pid=0x"
            << libevdev_get_id_product(dev) << std::dec << "\n";
  std::cerr << "[g29_reader] EV_ABS (axes) code  kernel_name  min..max\n";
  for (int code = 0; code <= ABS_MAX; ++code) {
    if (!libevdev_has_event_code(dev, EV_ABS, code)) {
      continue;
    }
    const auto* info = libevdev_get_abs_info(dev, code);
    const char* name = libevdev_event_code_get_name(EV_ABS, code);
    std::cerr << "  " << code << "  " << (name != nullptr ? name : "?");
    if (info != nullptr) {
      std::cerr << "  " << info->minimum << ".." << info->maximum;
    }
    std::cerr << "\n";
  }
  std::cerr << "[g29_reader] EV_KEY (buttons) code  kernel_name\n";
  for (int code = 0; code <= KEY_MAX; ++code) {
    if (!libevdev_has_event_code(dev, EV_KEY, code)) {
      continue;
    }
    const char* name = libevdev_event_code_get_name(EV_KEY, code);
    std::cerr << "  " << code << "  " << (name != nullptr ? name : "?") << "\n";
  }
  std::cerr << "[g29_reader] kernel names are NOT Logitech labels. "
               "Press one control with --print-raw to match code → JSON field.\n";

  libevdev_free(dev);
  close(fd);
  return 0;
}

}  // namespace cockpit
