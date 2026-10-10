#include "classic_bt.h"
#include "esp32-hal-bt.h"
#include "esp_spp_api.h"
#include <cstring>

#include "esphome/core/log.h"

namespace classic_bt_component {

static const char *const TAG = "classic_bt";

static ClassicBluetooth *s_instance = nullptr;

static constexpr uint32_t START_RETRY_INTERVAL_MS = 2000;
static constexpr uint32_t CONNECT_RETRY_INTERVAL_MS = 15000;
static constexpr uint32_t CONNECT_TIMEOUT_MS = 10000;


void ClassicBluetooth::setup() {
  ESP_LOGI(TAG, "Initializing Classic Bluetooth SPP transport");
  if (!enabled_ || !address_valid_) {
    ESP_LOGW(TAG, "Bluetooth transport disabled or remote address invalid");
    return;
  }
  start_bluetooth_();
  connect();
}

void ClassicBluetooth::loop() {
  if (!startup_status_logged_ && millis() >= 10000) {
    startup_status_logged_ = true;
    ESP_LOGI(
        TAG,
        "Startup status: started=%s, SPP ready=%s, connected=%s",
        started_ ? "yes" : "no",
        spp_ready_ ? "yes" : "no",
        bluetooth_.connected() ? "yes" : "no");
  }

  if (!enabled_ || !address_valid_) {
    return;
  }
  if (!started_) {
    if (millis() - last_start_attempt_ >= START_RETRY_INTERVAL_MS) {
      start_bluetooth_();
    }
    return;
  }

  if (bluetooth_.connected()) {
    connecting_ = false;
    return;
  }

  if (!spp_ready_) {
    return;
  }

  if (connecting_) {
    const uint32_t now = millis();
    if (now - connecting_since_ < CONNECT_TIMEOUT_MS) {
      return;
    }

    connecting_ = false;
    last_connect_attempt_ = now;
    ESP_LOGW(TAG, "SPP connection timed out");
  }

  const uint32_t now = millis();
  if (connect_attempted_ && now - last_connect_attempt_ < CONNECT_RETRY_INTERVAL_MS) {
    return;
  }

  connect();
}


void ClassicBluetooth::connect() {
  if (!enabled_ || !address_valid_) {
    return;
  }
  const uint32_t now = millis();

  start_bluetooth_();

  if (!started_) {
    return;
  }

  if (!spp_ready_) {
    if (debug_logging_) {
      ESP_LOGD(TAG, "Connection deferred: SPP is not ready");
    }
    return;
  }

  if (bluetooth_.connected() || connecting_) {
    return;
  }

  if (connect_attempted_ && now - last_connect_attempt_ < CONNECT_RETRY_INTERVAL_MS) {
    return;
  }

  connect_attempted_ = true;
  last_connect_attempt_ = now;
  connecting_since_ = now;
  connecting_ = true;

  if (debug_logging_) {
    ESP_LOGD(TAG, "Starting SPP service discovery");
  }

  const esp_err_t result = esp_spp_start_discovery(remote_address_);
  if (result != ESP_OK) {
    connecting_ = false;
    last_connect_attempt_ = millis();
    ESP_LOGW(TAG, "Failed to start SPP service discovery: 0x%X", static_cast<unsigned>(result));
    return;
  }
}


bool ClassicBluetooth::is_connected() {
  return enabled_ && address_valid_ && started_ && bluetooth_.connected();
}


Stream *ClassicBluetooth::get_stream() {
  return &bluetooth_;
}


void ClassicBluetooth::set_enabled(bool enabled) {
  enabled_ = enabled;
}


void ClassicBluetooth::set_address(const std::string &address) {
  address_valid_ = false;
  uint8_t parsed[6];

  if (!parse_mac_(address, parsed)) {
    ESP_LOGE(TAG, "Invalid Bluetooth MAC address: %s", address.c_str());
    return;
  }

  std::memcpy(
      remote_address_,
      parsed,
      sizeof(remote_address_));
  address_valid_ = true;
}

static int hex_value(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  return -1;
}

bool ClassicBluetooth::parse_mac_(const std::string &address, uint8_t *result) {
  if (result == nullptr || address.size() != 17) {
    return false;
  }

  for (size_t i = 0; i < 6; i++) {
    const size_t offset = i * 3;
    const int high = hex_value(address[offset]);
    const int low = hex_value(address[offset + 1]);
    if (high < 0 || low < 0 || (i < 5 && address[offset + 2] != ':')) {
      return false;
    }

    result[i] = static_cast<uint8_t>((high << 4) | low);
  }

  return true;
}

void ClassicBluetooth::set_local_name(const std::string &name) {
  device_name_ = name;
}


void ClassicBluetooth::set_auto_accept_pairing(bool enabled) {
  auto_accept_pairing_ = enabled;
}


void ClassicBluetooth::set_debug_logging(bool enabled) {
  debug_logging_ = enabled;
}


void ClassicBluetooth::start_bluetooth_() {
  if (started_) {
    return;
  }

  last_start_attempt_ = millis();

  if (!btStarted()) {
    if (!btStart()) {
        ESP_LOGE(TAG, "Failed to start Bluetooth controller");
      return;
    }
  }

  bluetooth_.enableSSP(true, true);

  bluetooth_.onConfirmRequest(
      [this](uint32_t num_val) {
        this->bt_confirm_request_(num_val);
      });

  bluetooth_.onAuthComplete(
      [this](bool success) {
        this->bt_auth_complete_(success);
      });

  s_instance = this;

  const esp_err_t callback_result =
      bluetooth_.register_callback(&ClassicBluetooth::spp_callback_);
  if (callback_result != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register BluetoothSerial callback: 0x%X", static_cast<unsigned>(callback_result));
    return;
  }

  // Keep the one-argument call; outbound SPP uses the ESP-IDF master API below.
  const bool result =
      bluetooth_.begin(device_name_.c_str());

  if (!result) {
    ESP_LOGE(TAG, "BluetoothSerial::begin() failed");
    return;
  }

  started_ = true;
}


void ClassicBluetooth::spp_callback_(
    esp_spp_cb_event_t event,
    esp_spp_cb_param_t *param) {

  if (s_instance == nullptr || param == nullptr) {
    return;
  }

  s_instance->handle_spp_event_(event, param);
}


void ClassicBluetooth::handle_spp_event_(
    esp_spp_cb_event_t event,
    esp_spp_cb_param_t *param) {
  switch (event) {
    case ESP_SPP_INIT_EVT:
      if (debug_logging_) {
        ESP_LOGD(TAG, "SPP initialized");
      }
      break;

    case ESP_SPP_START_EVT:
      spp_ready_ = true;
      if (debug_logging_) {
        ESP_LOGD(TAG, "SPP ready for connections");
      }
      break;

    case ESP_SPP_DISCOVERY_COMP_EVT: {
      if (!connecting_) {
        break;
      }
      if (param->disc_comp.status != ESP_SPP_SUCCESS ||
          param->disc_comp.scn_num == 0) {
        connecting_ = false;
        last_connect_attempt_ = millis();
        ESP_LOGW(TAG, "Remote device has no available SPP channel (status=%d)", param->disc_comp.status);
        break;
      }

      if (debug_logging_) {
        ESP_LOGD(TAG, "SPP channel found: %u", param->disc_comp.scn[0]);
      }
      const esp_err_t result = esp_spp_connect(
          static_cast<esp_spp_sec_t>(ESP_SPP_SEC_AUTHENTICATE | ESP_SPP_SEC_ENCRYPT),
          ESP_SPP_ROLE_MASTER,
          param->disc_comp.scn[0],
          remote_address_);
      if (result != ESP_OK) {
        connecting_ = false;
        last_connect_attempt_ = millis();
        ESP_LOGW(TAG, "Failed to start SPP connection: 0x%X", static_cast<unsigned>(result));
      }
      break;
    }

    case ESP_SPP_OPEN_EVT:
      connecting_ = false;
      last_connect_attempt_ = millis();
      if (param->open.status != ESP_SPP_SUCCESS) {
        ESP_LOGW(TAG, "SPP connection failed to open: status=%d", param->open.status);
        break;
      }
      ESP_LOGI(TAG, "SPP connection opened");
      if (debug_logging_) {
        ESP_LOGD(TAG, "SPP handle=%lu", static_cast<unsigned long>(param->open.handle));
      }
      break;

    case ESP_SPP_CLOSE_EVT:
      connecting_ = false;
      last_connect_attempt_ = millis();

        ESP_LOGW(TAG, "SPP connection closed");
      break;

    default:
      break;
  }
}


void ClassicBluetooth::bt_confirm_request_(
    uint32_t num_val) {
  if (auto_accept_pairing_) {
    if (debug_logging_) {
      ESP_LOGD(TAG, "Automatically accepting Bluetooth SSP confirmation: %06lu", static_cast<unsigned long>(num_val));
    }
  } else {
    ESP_LOGW(TAG, "Rejecting Bluetooth SSP confirmation request");
  }
  bluetooth_.confirmReply(auto_accept_pairing_);
}


void ClassicBluetooth::bt_auth_complete_(
    bool success) {
  if (!success) {
    ESP_LOGW(TAG, "Bluetooth authentication failed");
  } else if (debug_logging_) {
    ESP_LOGD(TAG, "Bluetooth authentication succeeded");
  }
}

}  // namespace classic_bt_component
