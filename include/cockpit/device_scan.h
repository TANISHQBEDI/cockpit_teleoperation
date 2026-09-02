#ifndef COCKPIT_DEVICE_SCAN_H_
#define COCKPIT_DEVICE_SCAN_H_

#include <string>
#include <vector>

namespace cockpit {

struct InputDeviceInfo {
  std::string name;
  std::string event_path;
  std::string vendor;   // hex, e.g. "046d"
  std::string product;  // hex, e.g. "c24f"
};

std::vector<InputDeviceInfo> ScanInputDevices();

// Prefers /dev/input/by-id *G29*event-joystick, then /proc/bus/input/devices
// matching 046d:c24f. Empty if not found. Warns on 046d:c260 (PS4 mode).
std::string FindG29EventPath();

void PrintDeviceList();

// Kernel-advertised EV_ABS / EV_KEY codes for this event node (not our mapping).
// Returns 0 on success.
int PrintDeviceCaps(const std::string& event_path);

}  // namespace cockpit

#endif  // COCKPIT_DEVICE_SCAN_H_
