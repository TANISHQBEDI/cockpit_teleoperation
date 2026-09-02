#include "cockpit/mqtt_publisher.h"

namespace cockpit {

StubMqttPublisher::StubMqttPublisher(std::string broker_uri)
    : broker_uri_(std::move(broker_uri)) {}

void StubMqttPublisher::Publish(std::string_view /*topic*/,
                                std::string_view /*payload*/) {
  ++published_count_;
}

}  // namespace cockpit
