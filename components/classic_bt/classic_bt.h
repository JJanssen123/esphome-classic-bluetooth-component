#pragma once

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <string>

#include "esp_spp_api.h"

#include "esphome/core/component.h"

namespace classic_bt_component {

class ClassicBluetooth : public esphome::Component {
 public:
  void setup() override;
  void loop() override;

  void connect();

  bool is_connected();

  Stream *get_stream();

  void set_enabled(bool enabled);
  void set_address(const std::string &address);
  void set_local_name(const std::string &name);
  void set_auto_accept_pairing(bool enabled);
  void set_debug_logging(bool enabled);

 protected:
  BluetoothSerial bluetooth_;

  uint8_t remote_address_[6]{};

  std::string device_name_ = "ESP32-Classic-Bluetooth";

  bool enabled_{true};
  bool address_valid_{false};
  bool started_{false};
  bool spp_ready_{false};
  bool connecting_{false};
  bool connect_attempted_{false};
  bool auto_accept_pairing_{true};
  bool debug_logging_{false};

  uint32_t last_start_attempt_{0};
  uint32_t last_connect_attempt_{0};
  uint32_t connecting_since_{0};

  void start_bluetooth_();

  void bt_confirm_request_(uint32_t num_val);
  void bt_auth_complete_(bool success);

  bool parse_mac_(
      const std::string &address,
      uint8_t *result);

  static void spp_callback_(
      esp_spp_cb_event_t event,
      esp_spp_cb_param_t *param);

  void handle_spp_event_(
      esp_spp_cb_event_t event,
      esp_spp_cb_param_t *param);

};

}  // namespace classic_bt_component
