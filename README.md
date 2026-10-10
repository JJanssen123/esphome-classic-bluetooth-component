# ESPHome Classic Bluetooth SPP Component

An ESPHome external component for connecting an ESP32 to a remote Bluetooth Classic device over SPP/RFCOMM. It exposes the connection as an Arduino `Stream` and provides a connection-state check.

This component supports Bluetooth Classic SPP, not BLE/GATT or other Bluetooth profiles.

## Requirements

- An ESP32 target using the Arduino framework.
- Bluetooth Classic, Bluedroid, and SPP enabled in the ESP32 framework configuration.
- Only one `classic_bt` instance per ESP32. The ESP-IDF SPP callback is routed through a singleton.

For ESPHome configurations that need explicit Classic Bluetooth settings:

```yaml
esp32:
  framework:
    type: arduino
    sdkconfig_options:
      CONFIG_BT_CLASSIC_ENABLED: "y"
      CONFIG_BT_BLUEDROID_ENABLED: "y"
      CONFIG_BT_SPP_ENABLED: "y"
```

## Installation

Add the repository as an external component:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/JJanssen123/esphome-classic-bluetooth-component
      ref: main
    components: [classic_bt]
```

## Configuration

```yaml
classic_bt:
  id: bt_transport
  address: "00:11:22:33:44:55"
  local_name: "ESP32-Classic-Bluetooth"
  enabled: true
  auto_accept_pairing: true
  debug_logging: false
```

`address` is the remote device's Bluetooth MAC address. `local_name` is the name advertised by the ESP32. `enabled` defaults to `true`, `auto_accept_pairing` defaults to `true`, and `debug_logging` defaults to `false`.

When `auto_accept_pairing` is false, new SSP confirmation requests are rejected. This component connects to the peer as an SPP master and requires authentication and encryption.

## Transport API

Use `get_stream()` to pass the transport to another component and `is_connected()` to check whether the SPP link is open:

```yaml
some_component:
  stream: !lambda |-
    return id(bt_transport).get_stream();
  is_connected: !lambda |-
    return id(bt_transport).is_connected();
```