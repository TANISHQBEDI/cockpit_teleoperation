#ifndef COCKPIT_MQTT_PUBLISHER_H_
#define COCKPIT_MQTT_PUBLISHER_H_

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>

struct mosquitto;

namespace cockpit {

class MqttPublisher {
 public:
  virtual ~MqttPublisher() = default;
  virtual void Publish(std::string_view topic, std::string_view payload) = 0;
  virtual std::uint64_t published_count() const = 0;
  virtual bool connected() const = 0;
};

class StubMqttPublisher : public MqttPublisher {
 public:
  explicit StubMqttPublisher(std::string broker_uri);

  void Publish(std::string_view topic, std::string_view payload) override;
  std::uint64_t published_count() const override { return published_count_; }
  bool connected() const override { return false; }

 private:
  std::string broker_uri_;
  std::uint64_t published_count_ = 0;
};

// MQTT 3.1.1 client for the existing VDA 5050 bus (any broker product).
// libmosquitto is the C client library, not a requirement that the bus is
// Eclipse Mosquitto. QoS 0, no retain.
class MosquittoMqttPublisher : public MqttPublisher {
 public:
  MosquittoMqttPublisher(std::string broker_uri, std::string client_id,
                         std::string username, std::string password);
  ~MosquittoMqttPublisher() override;

  MosquittoMqttPublisher(const MosquittoMqttPublisher&) = delete;
  MosquittoMqttPublisher& operator=(const MosquittoMqttPublisher&) = delete;

  void Publish(std::string_view topic, std::string_view payload) override;
  std::uint64_t published_count() const override {
    return published_count_.load();
  }
  bool connected() const override { return connected_.load(); }

 private:
  static void OnConnect(mosquitto* mosq, void* obj, int rc);
  static void OnDisconnect(mosquitto* mosq, void* obj, int rc);

  std::string broker_uri_;
  std::string host_;
  int port_ = 1883;
  mosquitto* mosq_ = nullptr;
  std::atomic<bool> connected_{false};
  std::atomic<bool> stopping_{false};
  std::atomic<std::uint64_t> published_count_{0};
};

}  // namespace cockpit

#endif  // COCKPIT_MQTT_PUBLISHER_H_
