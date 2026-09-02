#include "cockpit/mqtt_publisher.h"

#include <mosquitto.h>

#include <iostream>
#include <mutex>
#include <string>

namespace cockpit {
namespace {

void InitMosquittoOnce() {
  static std::once_flag flag;
  std::call_once(flag, []() { mosquitto_lib_init(); });
}

void ParseBrokerUri(std::string uri, std::string* host, int* port) {
  *port = 1883;
  for (const char* prefix : {"tcp://", "mqtt://"}) {
    const std::string p = prefix;
    if (uri.rfind(p, 0) == 0) {
      uri = uri.substr(p.size());
      break;
    }
  }
  const auto colon = uri.rfind(':');
  if (colon != std::string::npos && uri.find(']') == std::string::npos) {
    *host = uri.substr(0, colon);
    try {
      *port = std::stoi(uri.substr(colon + 1));
    } catch (...) {
      *port = 1883;
    }
  } else {
    *host = uri;
  }
  if (host->empty()) {
    *host = "127.0.0.1";
  }
}

}  // namespace

StubMqttPublisher::StubMqttPublisher(std::string broker_uri)
    : broker_uri_(std::move(broker_uri)) {}

void StubMqttPublisher::Publish(std::string_view /*topic*/,
                                std::string_view /*payload*/) {
  ++published_count_;
}

MosquittoMqttPublisher::MosquittoMqttPublisher(std::string broker_uri,
                                               std::string client_id,
                                               std::string username,
                                               std::string password)
    : broker_uri_(std::move(broker_uri)) {
  InitMosquittoOnce();
  ParseBrokerUri(broker_uri_, &host_, &port_);

  mosq_ = mosquitto_new(client_id.c_str(), /*clean_session=*/true, this);
  if (mosq_ == nullptr) {
    std::cerr << "[g29_reader] mosquitto_new failed\n";
    return;
  }
  if (!username.empty()) {
    const int auth_rc = mosquitto_username_pw_set(
        mosq_, username.c_str(),
        password.empty() ? nullptr : password.c_str());
    if (auth_rc != MOSQ_ERR_SUCCESS) {
      std::cerr << "[g29_reader] mqtt auth: " << mosquitto_strerror(auth_rc)
                << "\n";
    }
  }
  mosquitto_connect_callback_set(mosq_, &MosquittoMqttPublisher::OnConnect);
  mosquitto_disconnect_callback_set(mosq_,
                                    &MosquittoMqttPublisher::OnDisconnect);
  mosquitto_reconnect_delay_set(mosq_, 1, 8, true);

  const int rc =
      mosquitto_connect_async(mosq_, host_.c_str(), port_, /*keepalive=*/30);
  if (rc != MOSQ_ERR_SUCCESS) {
    std::cerr << "[g29_reader] mosquitto_connect_async(" << host_ << ":"
              << port_ << "): " << mosquitto_strerror(rc) << "\n";
  }
  const int loop_rc = mosquitto_loop_start(mosq_);
  if (loop_rc != MOSQ_ERR_SUCCESS) {
    std::cerr << "[g29_reader] mosquitto_loop_start: "
              << mosquitto_strerror(loop_rc) << "\n";
  }
  std::cerr << "[g29_reader] mqtt connecting " << host_ << ":" << port_
            << " client_id=" << client_id << "\n";
}

MosquittoMqttPublisher::~MosquittoMqttPublisher() {
  stopping_.store(true);
  if (mosq_ == nullptr) {
    return;
  }
  mosquitto_loop_stop(mosq_, /*force=*/true);
  mosquitto_disconnect(mosq_);
  mosquitto_destroy(mosq_);
  mosq_ = nullptr;
}

void MosquittoMqttPublisher::OnConnect(mosquitto* /*mosq*/, void* obj, int rc) {
  auto* self = static_cast<MosquittoMqttPublisher*>(obj);
  self->connected_.store(rc == 0);
  if (rc == 0) {
    std::cerr << "[g29_reader] mqtt connected " << self->host_ << ":"
              << self->port_ << "\n";
  } else {
    std::cerr << "[g29_reader] mqtt connect rc=" << rc << "\n";
  }
}

void MosquittoMqttPublisher::OnDisconnect(mosquitto* /*mosq*/, void* obj,
                                          int /*rc*/) {
  auto* self = static_cast<MosquittoMqttPublisher*>(obj);
  self->connected_.store(false);
  if (!self->stopping_.load()) {
    std::cerr << "[g29_reader] mqtt disconnected, will retry\n";
  }
}

void MosquittoMqttPublisher::Publish(std::string_view topic,
                                     std::string_view payload) {
  if (mosq_ == nullptr || !connected_.load()) {
    return;
  }
  const int rc = mosquitto_publish(
      mosq_, nullptr, topic.data(), static_cast<int>(payload.size()),
      payload.data(), /*qos=*/0, /*retain=*/false);
  if (rc == MOSQ_ERR_SUCCESS) {
    published_count_.fetch_add(1);
  }
}

}  // namespace cockpit
