# ESPHome Classic Bluetooth SPP Component

An ESPHome external component for connecting an ESP32 to a remote Bluetooth Classic device over SPP/RFCOMM. It exposes the connection as an Arduino `Stream` and provides a connection-state check.

This component supports Bluetooth Classic SPP, not BLE/GATT or other Bluetooth profiles.

## Requirements

- An ESP32 target using the Arduino framework.
- Bluetooth Classic, Bluedroid, and SPP enabled in the ESP32 framework configuration.
- Only one `classic_bt` instance per ESP32. The ESP-IDF SPP callback is routed through a singleton.

## ESP32 setup and build flags

Enable Classic Bluetooth, Bluedroid, and SPP in the framework, and pass the Classic/SPP compile-time defines under `esphome.build_flags`:

```yaml
esphome:
  build_flags:
    - "-DCONFIG_BT_CLASSIC_ENABLED=1"
    - "-DCONFIG_BT_SPP_ENABLED=1"

esp32:
  framework:
    type: arduino
    sdkconfig_options:
      CONFIG_BT_CLASSIC_ENABLED: "y"
      CONFIG_BT_BLUEDROID_ENABLED: "y"
      CONFIG_BT_SPP_ENABLED: "y"
```

These settings are complementary: `sdkconfig_options` enables the ESP-IDF features, while the two `build_flags` define the corresponding compile-time macros.

The following build flags are optional but might be required by this component:

- `"-DUSE_ESP32_BLE_SERVER=false"` disables the ESPHome BLE server. Use it only if the device does not need that BLE server.
- `"-DBTA_DM_LINK_POLICY_SETTINGS=0x000B"` changes the Bluedroid link policy globally to omit Classic ACL sniff mode. Add it only if that behavior is specifically wanted; it is not a per-component setting. **It might be necessary to override sniff mode that causes reboot.**

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
