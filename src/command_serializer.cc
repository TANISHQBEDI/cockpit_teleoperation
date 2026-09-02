#include "cockpit/command_serializer.h"

#include <cstdio>
#include <time.h>

namespace cockpit {
namespace {

void FormatUtcIso8601Ms(char* out, std::size_t n) {
  timespec ts{};
  clock_gettime(CLOCK_REALTIME, &ts);
  tm tm{};
  gmtime_r(&ts.tv_sec, &tm);
  const int ms = static_cast<int>(ts.tv_nsec / 1'000'000);
  std::snprintf(out, n, "\"%04d-%02d-%02dT%02d:%02d:%02d.%03dZ\"",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                tm.tm_min, tm.tm_sec, ms);
}

const char* BoolJson(bool v) { return v ? "true" : "false"; }

}  // namespace

CommandSerializer::CommandSerializer(Identity identity)
    : identity_(std::move(identity)) {}

std::string_view CommandSerializer::Serialize(const G29State& s,
                                              std::uint32_t header_id) {
  char ts[64];
  FormatUtcIso8601Ms(ts, sizeof(ts));

  const int n = std::snprintf(
      buf_, sizeof(buf_),
      "{"
      "\"header\":{"
      "\"headerId\":%u,\"timestamp\":%s,"
      "\"version\":\"%s\",\"manufacturer\":\"%s\",\"serialNumber\":\"%s\""
      "},"
      "\"voltageVariedInputs\":{"
      "\"steeringAngleDeg\":%.3f,\"acceleratorPedal\":%.4f,"
      "\"brakePedal\":%.4f,\"clutchPedal\":%.4f,\"rotaryDialPosition\":%d"
      "},"
      "\"digitalButtons\":{"
      "\"leftPaddleDownshift\":%s,\"rightPaddleUpshift\":%s,"
      "\"crossX\":%s,\"circleO\":%s,\"square\":%s,\"triangle\":%s,"
      "\"l2Button\":%s,\"r2Button\":%s,\"l3Button\":%s,\"r3Button\":%s,"
      "\"dpadUp\":%s,\"dpadDown\":%s,\"dpadLeft\":%s,\"dpadRight\":%s,"
      "\"plusButton\":%s,\"minusButton\":%s,\"rotaryEnterButton\":%s,"
      "\"psButton\":%s,\"shareButton\":%s,\"optionButton\":%s"
      "},"
      "\"optionalHShifter\":{\"shifterGear\":%d}"
      "}",
      header_id, ts, identity_.protocol_version.c_str(),
      identity_.manufacturer.c_str(), identity_.serial_number.c_str(),
      s.steering_angle_deg, s.accelerator_pedal, s.brake_pedal, s.clutch_pedal,
      s.rotary_dial_position, BoolJson(s.left_paddle_downshift),
      BoolJson(s.right_paddle_upshift), BoolJson(s.cross_x),
      BoolJson(s.circle_o), BoolJson(s.square), BoolJson(s.triangle),
      BoolJson(s.l2_button), BoolJson(s.r2_button), BoolJson(s.l3_button),
      BoolJson(s.r3_button), BoolJson(s.dpad_up), BoolJson(s.dpad_down),
      BoolJson(s.dpad_left), BoolJson(s.dpad_right), BoolJson(s.plus_button),
      BoolJson(s.minus_button), BoolJson(s.rotary_enter_button),
      BoolJson(s.ps_button), BoolJson(s.share_button),
      BoolJson(s.option_button), s.shifter_gear);

  if (n < 0) {
    buf_[0] = '\0';
    return {};
  }
  if (static_cast<std::size_t>(n) >= sizeof(buf_)) {
    buf_[sizeof(buf_) - 1] = '\0';
    return std::string_view(buf_, sizeof(buf_) - 1);
  }
  return std::string_view(buf_, static_cast<std::size_t>(n));
}

std::string_view CommandSerializer::SerializePretty(const G29State& s,
                                                   std::uint32_t header_id) {
  char ts[64];
  FormatUtcIso8601Ms(ts, sizeof(ts));

  const int n = std::snprintf(
      buf_, sizeof(buf_),
      "{\n"
      "  \"header\": {\n"
      "    \"headerId\": %u,\n"
      "    \"timestamp\": %s,\n"
      "    \"version\": \"%s\",\n"
      "    \"manufacturer\": \"%s\",\n"
      "    \"serialNumber\": \"%s\"\n"
      "  },\n"
      "  \"voltageVariedInputs\": {\n"
      "    \"steeringAngleDeg\": %.3f,\n"
      "    \"acceleratorPedal\": %.4f,\n"
      "    \"brakePedal\": %.4f,\n"
      "    \"clutchPedal\": %.4f,\n"
      "    \"rotaryDialPosition\": %d\n"
      "  },\n"
      "  \"digitalButtons\": {\n"
      "    \"leftPaddleDownshift\": %s,\n"
      "    \"rightPaddleUpshift\": %s,\n"
      "    \"crossX\": %s,\n"
      "    \"circleO\": %s,\n"
      "    \"square\": %s,\n"
      "    \"triangle\": %s,\n"
      "    \"l2Button\": %s,\n"
      "    \"r2Button\": %s,\n"
      "    \"l3Button\": %s,\n"
      "    \"r3Button\": %s,\n"
      "    \"dpadUp\": %s,\n"
      "    \"dpadDown\": %s,\n"
      "    \"dpadLeft\": %s,\n"
      "    \"dpadRight\": %s,\n"
      "    \"plusButton\": %s,\n"
      "    \"minusButton\": %s,\n"
      "    \"rotaryEnterButton\": %s,\n"
      "    \"psButton\": %s,\n"
      "    \"shareButton\": %s,\n"
      "    \"optionButton\": %s\n"
      "  },\n"
      "  \"optionalHShifter\": {\n"
      "    \"shifterGear\": %d\n"
      "  }\n"
      "}\n",
      header_id, ts, identity_.protocol_version.c_str(),
      identity_.manufacturer.c_str(), identity_.serial_number.c_str(),
      s.steering_angle_deg, s.accelerator_pedal, s.brake_pedal, s.clutch_pedal,
      s.rotary_dial_position, BoolJson(s.left_paddle_downshift),
      BoolJson(s.right_paddle_upshift), BoolJson(s.cross_x),
      BoolJson(s.circle_o), BoolJson(s.square), BoolJson(s.triangle),
      BoolJson(s.l2_button), BoolJson(s.r2_button), BoolJson(s.l3_button),
      BoolJson(s.r3_button), BoolJson(s.dpad_up), BoolJson(s.dpad_down),
      BoolJson(s.dpad_left), BoolJson(s.dpad_right), BoolJson(s.plus_button),
      BoolJson(s.minus_button), BoolJson(s.rotary_enter_button),
      BoolJson(s.ps_button), BoolJson(s.share_button),
      BoolJson(s.option_button), s.shifter_gear);

  if (n < 0) {
    buf_[0] = '\0';
    return {};
  }
  if (static_cast<std::size_t>(n) >= sizeof(buf_)) {
    buf_[sizeof(buf_) - 1] = '\0';
    return std::string_view(buf_, sizeof(buf_) - 1);
  }
  return std::string_view(buf_, static_cast<std::size_t>(n));
}

}  // namespace cockpit
