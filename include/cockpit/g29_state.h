#ifndef COCKPIT_G29_STATE_H_
#define COCKPIT_G29_STATE_H_

#include <cstdint>
#include <string>

namespace cockpit {

// Mapped G29 snapshot. Field names match command.json (VDA 5050 camelCase).
struct G29State {
  double steering_angle_deg = 0.0;
  double accelerator_pedal = 0.0;
  double brake_pedal = 0.0;
  double clutch_pedal = 0.0;
  int rotary_dial_position = 0;

  bool left_paddle_downshift = false;
  bool right_paddle_upshift = false;
  bool cross_x = false;
  bool circle_o = false;
  bool square = false;
  bool triangle = false;
  bool l2_button = false;
  bool r2_button = false;
  bool l3_button = false;
  bool r3_button = false;
  bool dpad_up = false;
  bool dpad_down = false;
  bool dpad_left = false;
  bool dpad_right = false;
  bool plus_button = false;
  bool minus_button = false;
  bool rotary_enter_button = false;
  bool ps_button = false;
  bool share_button = false;
  bool option_button = false;

  int shifter_gear = 0;

  bool connected = false;
  std::string block_reason;  // empty when transmitting
};

struct Identity {
  std::string manufacturer = "ANSCER";
  std::string serial_number = "AR001";
  std::string protocol_version = "v2.0.0";
};

}  // namespace cockpit

#endif  // COCKPIT_G29_STATE_H_
