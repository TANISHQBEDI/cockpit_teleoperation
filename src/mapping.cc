#include "cockpit/mapping.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <string>

namespace cockpit {
namespace {

std::string Trim(std::string s) {
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

bool ParseBool(const std::string& v) {
  return v == "1" || v == "true" || v == "TRUE" || v == "yes";
}

}  // namespace

MappingConfig LoadMappingConfig(const std::string& path) {
  MappingConfig cfg;
  if (path.empty()) {
    return cfg;
  }

  std::ifstream in(path);
  if (!in) {
    std::cerr << "[g29_reader] mapping file not found (" << path
              << "), using built-in defaults\n";
    return cfg;
  }

  std::string line;
  int line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    line = Trim(line);
    if (line.empty() || line.front() == '#') {
      continue;
    }
    const auto eq = line.find('=');
    if (eq == std::string::npos) {
      std::cerr << "[g29_reader] " << path << ":" << line_no
                << ": ignoring line without '='\n";
      continue;
    }
    const std::string key = Trim(line.substr(0, eq));
    const std::string value = Trim(line.substr(eq + 1));

    if (key == "manufacturer") {
      cfg.identity.manufacturer = value;
    } else if (key == "serial_number") {
      cfg.identity.serial_number = value;
    } else if (key == "protocol_version") {
      cfg.identity.protocol_version = value;
    } else if (key == "steering_range_deg") {
      cfg.steering_range_deg = std::stod(value);
    } else if (key == "invert_pedals") {
      cfg.invert_pedals = ParseBool(value);
    } else if (key == "steer_abs_code") {
      cfg.steer_abs_code = std::stoi(value);
    } else if (key == "accel_abs_code") {
      cfg.accel_abs_code = std::stoi(value);
    } else if (key == "brake_abs_code") {
      cfg.brake_abs_code = std::stoi(value);
    } else if (key == "clutch_abs_code") {
      cfg.clutch_abs_code = std::stoi(value);
    } else if (key == "hat_x_abs_code") {
      cfg.hat_x_abs_code = std::stoi(value);
    } else if (key == "hat_y_abs_code") {
      cfg.hat_y_abs_code = std::stoi(value);
    } else if (key == "btn_cross") {
      cfg.btn_cross = std::stoi(value);
    } else if (key == "btn_square") {
      cfg.btn_square = std::stoi(value);
    } else if (key == "btn_circle") {
      cfg.btn_circle = std::stoi(value);
    } else if (key == "btn_triangle") {
      cfg.btn_triangle = std::stoi(value);
    } else if (key == "btn_right_paddle") {
      cfg.btn_right_paddle = std::stoi(value);
    } else if (key == "btn_left_paddle") {
      cfg.btn_left_paddle = std::stoi(value);
    } else if (key == "btn_r2") {
      cfg.btn_r2 = std::stoi(value);
    } else if (key == "btn_l2") {
      cfg.btn_l2 = std::stoi(value);
    } else if (key == "btn_share") {
      cfg.btn_share = std::stoi(value);
    } else if (key == "btn_options") {
      cfg.btn_options = std::stoi(value);
    } else if (key == "btn_r3") {
      cfg.btn_r3 = std::stoi(value);
    } else if (key == "btn_l3") {
      cfg.btn_l3 = std::stoi(value);
    } else if (key == "btn_ps") {
      cfg.btn_ps = std::stoi(value);
    } else if (key == "btn_plus") {
      cfg.btn_plus = std::stoi(value);
    } else if (key == "btn_minus") {
      cfg.btn_minus = std::stoi(value);
    } else if (key == "btn_rotary_enter") {
      cfg.btn_rotary_enter = std::stoi(value);
    } else if (key == "mqtt_topic") {
      cfg.mqtt_topic = value;
    } else if (key == "mqtt_broker") {
      cfg.mqtt_broker = value;
    } else {
      std::cerr << "[g29_reader] " << path << ":" << line_no
                << ": unknown key '" << key << "'\n";
    }
  }
  return cfg;
}

void PrintMappingConfig(const MappingConfig& cfg) {
  std::cerr << "[g29_reader] mapping.conf (our guesses — edit if dump-caps/raw disagree)\n"
            << "  steer_abs_code=" << cfg.steer_abs_code
            << " accel_abs_code=" << cfg.accel_abs_code
            << " brake_abs_code=" << cfg.brake_abs_code
            << " clutch_abs_code=" << cfg.clutch_abs_code << "\n"
            << "  hat_x=" << cfg.hat_x_abs_code << " hat_y=" << cfg.hat_y_abs_code
            << " steering_range_deg=" << cfg.steering_range_deg
            << " invert_pedals=" << (cfg.invert_pedals ? "1" : "0") << "\n"
            << "  btn_cross=" << cfg.btn_cross << " square=" << cfg.btn_square
            << " circle=" << cfg.btn_circle << " triangle=" << cfg.btn_triangle
            << "\n  right_paddle=" << cfg.btn_right_paddle
            << " left_paddle=" << cfg.btn_left_paddle << " r2=" << cfg.btn_r2
            << " l2=" << cfg.btn_l2 << "\n  share=" << cfg.btn_share
            << " options=" << cfg.btn_options << " r3=" << cfg.btn_r3
            << " l3=" << cfg.btn_l3 << " ps=" << cfg.btn_ps << "\n  plus="
            << cfg.btn_plus << " minus=" << cfg.btn_minus
            << " rotary_enter=" << cfg.btn_rotary_enter << "\n";
}

}  // namespace cockpit
