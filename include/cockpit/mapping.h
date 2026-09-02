#ifndef COCKPIT_MAPPING_H_
#define COCKPIT_MAPPING_H_

#include <cstdint>
#include <string>

#include "cockpit/g29_state.h"

namespace cockpit {

struct MappingConfig {
  Identity identity;
  double steering_range_deg = 900.0;
  bool invert_pedals = true;

  int steer_abs_code = 0;     // ABS_X
  int accel_abs_code = 2;     // ABS_Z (verified Ubuntu G29)
  int brake_abs_code = 5;     // ABS_RZ
  int clutch_abs_code = 1;    // ABS_Y
  int hat_x_abs_code = 16;    // ABS_HAT0X
  int hat_y_abs_code = 17;    // ABS_HAT0Y

  int btn_cross = 288;
  int btn_square = 289;
  int btn_circle = 290;
  int btn_triangle = 291;
  int btn_right_paddle = 292;
  int btn_left_paddle = 293;
  int btn_r2 = 294;
  int btn_l2 = 295;
  int btn_share = 296;
  int btn_options = 297;
  int btn_r3 = 298;
  int btn_l3 = 299;
  int btn_ps = 302;
  int btn_plus = 307;
  int btn_minus = 308;
  int btn_rotary_enter = 309;

  std::string mqtt_topic = "uagv/v2/ANSCER/AR001/command";
  std::string mqtt_broker = "tcp://127.0.0.1:1883";
  std::string mqtt_username;
  std::string mqtt_password;
};

// Loads key=value config. Missing file → defaults. Unknown keys are ignored
// with a stderr warning so Ubuntu calibration edits stay additive.
MappingConfig LoadMappingConfig(const std::string& path);
void PrintMappingConfig(const MappingConfig& cfg);

}  // namespace cockpit

#endif  // COCKPIT_MAPPING_H_
