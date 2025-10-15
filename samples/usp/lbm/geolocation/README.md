# LoRaWAN Geolocation

This application demonstrates **LoRaWAN geolocation services** using LR1110/LR1120 transceivers with integrated GNSS and Wi-Fi scanning capabilities. It automatically performs geolocation scans and transmits the results via LoRaWAN for location tracking applications.

## Key Features

- **Dual Geolocation Methods**: GNSS (GPS + BeiDou) and Wi-Fi scanning
- **Automatic Scanning**: Periodic GNSS (2 min) and Wi-Fi (1 min) scans
- **Almanac Demodulation**: Enhanced GNSS performance with constellation data
- **Store and Forward**: Geolocation data buffering and transmission
- **Custom ADR Profile**: Optimized for geolocation transmission requirements
- **LED Indicators**: Visual feedback for scanning and transmission states
- **Keep-Alive Messages**: Regular status updates every 30 seconds

## Configuration

### Hardware Requirements

| Component     | Requirement                    |
|---------------|--------------------------------|
| Transceiver   | LR1110 or LR1120               |
| Antenna       | GNSS + LoRa antenna or dual antenna setup |
| Network       | LoRaWAN network with geolocation support |

### Network Credentials (Required)

Configure your LoRaWAN credentials in `boards/user_keys.overlay`:

```dts
/ {
    zephyr,user {
        user-lorawan-device-eui = <0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00>;
        user-lorawan-join-eui = <0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00>;
        user-lorawan-app-key = <0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
                               0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00>;
        user-lorawan-region = "EU_868";
    };
};
```

### Geolocation Parameters

| Parameter                        | Value | Description                      |
|----------------------------------|-------|----------------------------------|
| `GEOLOCATION_GNSS_SCAN_PERIOD_S` | `120` | GNSS scan interval               |
| `GEOLOCATION_WIFI_SCAN_PERIOD_S` | `60`  | Wi-Fi scan interval              |
| `KEEP_ALIVE_PERIOD_S`            | `30`  | Keep-alive message interval      |
| `KEEP_ALIVE_PORT`                | `2`   | LoRaWAN port for status messages |

## Compilation

```bash
west build --pristine --board nrf52840dk/nrf52840 --shield semtech_lr1110mb1xxs samples/usp/lbm/geolocation
```

## Usage

1. **Network Setup**: Configure LoRaWAN credentials and ensure network supports geolocation services
2. **Deploy Device**: Place in location with GNSS satellite and Wi-Fi access point visibility
3. **Operation**:
   - **Automatic Join**: Device joins LoRaWAN network on startup
   - **Almanac Download**: Begins downloading GNSS almanac for improved performance
   - **Periodic Scans**: Alternates between GNSS and Wi-Fi scanning
   - **Data Transmission**: Sends geolocation data via LoRaWAN uplinks

## Expected Output

### Initialization
```
[00:00:01.000,000] <inf> geolocation_sample: GEOLOCATION example is starting
[00:00:01.100,000] <inf> geolocation_sample: LR11XX FW: 0x0401, type: 0x01
[00:00:02.200,000] <inf> geolocation_sample: Event received: JOINED
```

### Scanning Operations
```
[00:00:30.300,000] <inf> geolocation_sample: Event received: GNSS_TERMINATED
[00:01:15.400,000] <inf> geolocation_sample: Event received: WIFI_TERMINATED
[00:01:20.500,000] <inf> geolocation_sample: GPS progress: 45%
[00:01:20.500,000] <inf> geolocation_sample: BDS progress: 23%
```

## Technical Notes

- **Constellation Support**: GPS and BeiDou satellites for improved accuracy
- **Custom ADR**: Optimized transmission parameters for geolocation payloads
- **Store and Forward**: Buffers scan results for reliable transmission
- **Firmware Validation**: Automatic check for compatible LR11XX firmware versions
- **LED Feedback**: TX LED pulses during scan completion and transmission events
