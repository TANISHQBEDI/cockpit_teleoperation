#include <signal.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "cockpit/command_serializer.h"
#include "cockpit/device_scan.h"
#include "cockpit/evdev_reader.h"
#include "cockpit/mapping.h"
#include "cockpit/mock_reader.h"
#include "cockpit/mqtt_publisher.h"

namespace {

volatile sig_atomic_t g_running = 1;

void OnSignal(int /*sig*/) { g_running = 0; }

void PrintUsage(const char* argv0) {
  std::cerr
      << "Usage: " << argv0 << " [options]\n"
      << "  --mock                 Synthetic G29 (Mac / CI, no USB)\n"
      << "  --device PATH|auto     evdev node; auto = 046d:c24f discovery\n"
      << "  --config PATH          Mapping file (default: built-in)\n"
      << "  --rate HZ              Snapshot rate (default: 500)\n"
      << "  --duration SEC         Exit after N seconds (0 = forever)\n"
      << "  --json-stdout          Compact command.json per tick on stdout\n"
      << "  --pretty               Indented JSON on stderr (watch in the terminal)\n"
      << "  --print-raw            Dump evdev type/code/name/value on stderr\n"
      << "  --dump-caps            Print kernel axis/button codes and exit\n"
      << "  --list-devices         Print /proc/bus/input/devices and exit\n"
      << "  --help\n";
}

}  // namespace

int main(int argc, char** argv) {
  using clock = std::chrono::steady_clock;

  bool mock = false;
  bool json_stdout = false;
  bool print_raw = false;
  bool pretty = false;
  bool dump_caps = false;
  int rate_hz = 500;
  double duration_sec = 0.0;
  std::string device = "auto";
  std::string config_path;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto need = [&](const char* name) -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "[g29_reader] missing value for " << name << "\n";
        std::exit(2);
      }
      return argv[++i];
    };
    if (arg == "--mock") {
      mock = true;
    } else if (arg == "--json-stdout") {
      json_stdout = true;
    } else if (arg == "--print-raw") {
      print_raw = true;
    } else if (arg == "--pretty") {
      pretty = true;
    } else if (arg == "--dump-caps") {
      dump_caps = true;
    } else if (arg == "--list-devices") {
      cockpit::PrintDeviceList();
      return 0;
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      return 0;
    } else if (arg == "--device") {
      device = need("--device");
    } else if (arg == "--config") {
      config_path = need("--config");
    } else if (arg == "--rate") {
      rate_hz = std::stoi(need("--rate"));
    } else if (arg == "--duration") {
      duration_sec = std::stod(need("--duration"));
    } else {
      std::cerr << "[g29_reader] unknown argument: " << arg << "\n";
      PrintUsage(argv[0]);
      return 2;
    }
  }

  if (rate_hz <= 0 || rate_hz > 2000) {
    std::cerr << "[g29_reader] --rate must be 1..2000\n";
    return 2;
  }

  signal(SIGINT, OnSignal);
  signal(SIGTERM, OnSignal);

  const auto mapping = cockpit::LoadMappingConfig(config_path);

  auto resolve_device = [&]() -> std::string {
    if (device != "auto") {
      return device;
    }
    std::string found = cockpit::FindG29EventPath();
    if (found.empty()) {
      std::cerr << "[g29_reader] no G29 (046d:c24f) found. "
                   "Use --list-devices, --device PATH, or --mock.\n";
      cockpit::PrintDeviceList();
    }
    return found;
  };

  if (dump_caps) {
    if (mock) {
      std::cerr << "[g29_reader] --dump-caps needs a real evdev device\n";
      return 2;
    }
    device = resolve_device();
    if (device.empty()) {
      return 1;
    }
    cockpit::PrintMappingConfig(mapping);
    return cockpit::PrintDeviceCaps(device);
  }

  cockpit::CommandSerializer serializer(mapping.identity);
  cockpit::StubMqttPublisher mqtt(mapping.mqtt_broker);

  std::unique_ptr<cockpit::InputSource> source;
  if (mock) {
    std::cerr << "[g29_reader] source=mock  mqtt=STUB (not connected) broker="
              << mapping.mqtt_broker << " topic=" << mapping.mqtt_topic << "\n";
    source = std::make_unique<cockpit::MockReader>(mapping);
  } else {
    device = resolve_device();
    if (device.empty()) {
      return 1;
    }
    std::cerr << "[g29_reader] source=evdev path=" << device
              << " mqtt=STUB (not connected) broker=" << mapping.mqtt_broker
              << " topic=" << mapping.mqtt_topic << "\n";
    source = std::make_unique<cockpit::EvdevReader>(device, mapping, print_raw);
  }

  const bool tty = isatty(STDERR_FILENO) == 1;
  if (!pretty && tty && (print_raw || json_stdout) && rate_hz <= 25) {
    pretty = true;
  }
  // Line-buffering off is for 500 Hz capture. Keep it for that path only;
  // otherwise stdout sits in a buffer and JSON appears late vs [raw].
  if (!pretty && !print_raw) {
    std::ios::sync_with_stdio(false);
  }
  if (pretty) {
    std::cerr << "[g29_reader] pretty JSON on stderr every 250 ms "
                 "(same stream as [raw])\n";
  }

  const auto period = std::chrono::nanoseconds(1'000'000'000 / rate_hz);
  auto next = clock::now();
  const auto t0 = next;
  auto stats_at = t0 + std::chrono::seconds(1);
  auto pretty_at = t0;

  std::uint32_t header_id = 1;
  std::uint64_t emitted = 0;
  std::uint64_t blocked = 0;
  std::uint64_t overruns = 0;
  std::uint64_t window_emitted = 0;

  std::cerr << "[g29_reader] looping at " << rate_hz << " Hz  "
            << "steering_range_deg=" << mapping.steering_range_deg << "\n";

  while (g_running) {
    if (duration_sec > 0.0) {
      const double elapsed = std::chrono::duration<double>(clock::now() - t0)
                                 .count();
      if (elapsed >= duration_sec) {
        break;
      }
    }

    source->Drain();
    const auto& state = source->state();

    if (!state.connected || !state.block_reason.empty()) {
      ++blocked;
    } else {
      const auto json = serializer.Serialize(state, header_id++);
      if (json_stdout) {
        std::cout << json << '\n' << std::flush;
      }
      mqtt.Publish(mapping.mqtt_topic, json);
      ++emitted;
      ++window_emitted;
      if (pretty && clock::now() >= pretty_at) {
        std::cerr << serializer.SerializePretty(state, header_id - 1)
                  << std::flush;
        pretty_at = clock::now() + std::chrono::milliseconds(250);
      }
    }

    next += period;
    const auto now = clock::now();
    if (now > next) {
      ++overruns;
      next = now;
    } else {
      std::this_thread::sleep_until(next);
    }

    if (now >= stats_at) {
      std::cerr << "[g29_reader] rate=" << window_emitted << " Hz"
                << " emitted=" << emitted << " blocked=" << blocked
                << " overruns=" << overruns
                << " mqtt_stub=" << mqtt.published_count()
                << " mqtt_connected=" << (mqtt.connected() ? "true" : "false")
                << " steer=" << state.steering_angle_deg << " deg"
                << " accel=" << state.accelerator_pedal;
      if (!state.block_reason.empty()) {
        std::cerr << " BLOCKED=" << state.block_reason;
      }
      std::cerr << "\n";
      window_emitted = 0;
      stats_at += std::chrono::seconds(1);
    }
  }

  if (json_stdout) {
    std::cout.flush();
  }
  std::cerr << "[g29_reader] stop emitted=" << emitted << " blocked=" << blocked
            << " overruns=" << overruns
            << " mqtt_stub=" << mqtt.published_count() << "\n";
  return 0;
}
