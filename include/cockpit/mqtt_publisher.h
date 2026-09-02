#ifndef COCKPIT_MQTT_PUBLISHER_H_
#define COCKPIT_MQTT_PUBLISHER_H_

#include <cstdint>
#include <string>
#include <string_view>

namespace cockpit {

// Step 1: interface only. Step 2 will add a real client (see README table).
class MqttPublisher {
 public:
  virtual ~MqttPublisher() = default;
  virtual void Publish(std::string_view topic, std::string_view payload) = 0;
  virtual std::uint64_t published_count() const = 0;
  virtual bool connected() const = 0;
};

// Counts publishes, opens no socket, never connects.
class StubMqttPublisher : public MqttPublisher {
 public:
  explicit StubMqttPublisher(std::string broker_uri);

  void Publish(std::string_view topic, std::string_view payload) override;
  std::uint64_t published_count() const override { return published_count_; }
  bool connected() const override { return false; }

  const std::string& broker_uri() const { return broker_uri_; }

 private:
  std::string broker_uri_;
  std::uint64_t published_count_ = 0;
};

}  // namespace cockpit

#endif  // COCKPIT_MQTT_PUBLISHER_H_
