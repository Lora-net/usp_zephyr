# Ranging Demo

This application demonstrates **high-precision distance measurement** between two LoRa devices using Time-of-Flight (ToF) ranging with frequency hopping. One device acts as a manager coordinating the ranging process, while the other acts as a subordinate responding to ranging requests. The system performs measurements across multiple frequency channels and computes a median distance for improved accuracy.

## Key Features

- **Frequency Hopping**: Uses different channels for robust measurements
- **Statistical Processing**: Computes median distance from multiple measurements
- **OLED Display Support**: Visual feedback with real-time distance and RF parameters
- **LED Indicators**: Visual status indication for TX/RX and ranging operations
- **Multiple Data Rates**: Optional support for different spreading factors and bandwidths
- **Continuous Mode**: Optional automatic continuous ranging
- **Periodic Uplink**: Optional periodic LoRa transmissions for network connectivity

### Operation Modes

- **Manager Mode**: Initiates ranging exchanges, coordinates frequency hopping, and calculates results
- **Subordinate Mode**: Responds to ranging requests and participates in frequency hopping sequence

### Ranging Process

The ranging process involves several phases:
1. **Configuration Exchange**: Manager sends ranging parameters to subordinate (using LoRa)
2. **Acknowledgment**: Subordinate confirms reception and readiness
3. **Frequency Hopping**: Both devices perform ranging on multiple channels
4. **Result Processing**: Manager computes median distance from all measurements

## Configuration

### Using CMake

| Parameter                     | Default Value                   | Description                                      |
|-------------------------------|---------------------------------|--------------------------------------------------|
| `RANGING_DEVICE_MODE`         | `1`                             | Device role: Subordinate (1) or Manager (2)      |
| `RF_FREQ_IN_HZ`               | `868100000`                     | Base operating frequency in Hz                   |
| `TX_OUTPUT_POWER_DBM`         | `14`                            | Transmit power in dBm                            |
| `LORA_SPREADING_FACTOR`       | `RAL_LORA_SF9`                  | LoRa spreading factor                            |
| `LORA_BANDWIDTH`              | `RAL_LORA_BW_500_KHZ`           | LoRa bandwidth (500 kHz)                         |
| `LORA_CODING_RATE`            | `RAL_LORA_CR_4_5`               | LoRa coding rate (4/5)                           |
| `LORA_PREAMBLE_LENGTH`        | `12`                            | Preamble length in symbols (critical for timing) |
| `LORA_PKT_LEN_MODE`           | `RAL_LORA_PKT_EXPLICIT`         | Packet length mode                               |
| `LORA_IQ`                     | `false`                         | IQ inversion (keep standard for calibration)     |
| `LORA_CRC`                    | `true`                          | Enable CRC                                       |
| `LORA_SYNCWORD`               | `LORA_PRIVATE_NETWORK_SYNCWORD` | LoRa sync word                                   |
| `ACTIVATE_MULTIPLE_DATA_RATE` | `false`                         | Enable multiple data rate testing                |
| `CONTINUOUS_RANGING`          | `false`                         | Enable continuous ranging mode                   |
| `PERIODIC_UPLINK_ENABLED`     | `false`                         | Enable periodic uplink transmissions             |
| `TX_PERIODICITY_IN_MS`        | `200000`                        | Periodic uplink interval (200 seconds)           |
| `PAYLOAD_LENGTH`              | `7`                             | Ranging payload length in bytes                  |

### Hardware Requirements

- **OLED Display**: Optional SSD1306 I2C display for visual feedback
- **User Button**: Required for manual ranging initiation
- **LEDs**: TX/RX status indicators

## Compilation

**Build manager (manager) device:**
```bash
west build --pristine --board nucleo_l476rg/stm32l476xx --shield semtech_lr2021mb1xxs samples/usp/sdk/ranging_demo -- -DCMAKE_C_FLAGS="-DRANGING_DEVICE_MODE=RANGING_DEVICE_MODE_MANAGER"
```

**Build subordinate (subordinate) device:**
```bash
west build --pristine --board nucleo_l476rg/stm32l476xx --shield semtech_lr2021mb1xxs samples/usp/sdk/ranging_demo -- -DCMAKE_C_FLAGS="-DRANGING_DEVICE_MODE=RANGING_DEVICE_MODE_SUBORDINATE"
```

```bash
west flash
```

## Usage

1. **Setup Two Devices**:
   - Flash one device as manager (`RANGING_DEVICE_MODE=RANGING_DEVICE_MODE_MANAGER`)
   - Flash second device as subordinate (`RANGING_DEVICE_MODE=RANGING_DEVICE_MODE_SUBORDINATE`)

2. **Position Devices**: Place devices at desired measurement distance

3. **Power On**: Start both devices (subordinate first recommended)

4. **Initiate Ranging**:
   - **Manual Mode**: Press user button on manager device
   - **Continuous Mode**: Ranging starts automatically if enabled

5. **Monitor Results**:
   - **UART Output**: Distance measurements and statistics
   - **OLED Display**: Real-time distance, SF, and BW (if available)
   - **LEDs**: TX/RX activity indication

6. **Distance Calculation**: Manager performs measurements across all frequency channels and computes median distance

## Expected Output

### Manager device output

```
[00:00:00.036,000] <inf> usp: ===== ranging and frequency hopping example =====
[00:00:00.040,000] <inf> usp: Running in ranging manager mode
[00:00:00.045,000] <inf> usp: Ranging hopping demo started
[00:00:00.050,000] <inf> usp: Manager Ranging config tx done  -> go to rx wait for subordinate answer
[00:00:01.200,000] <inf> usp: starting ranging exchange
[00:00:01.200,000] <inf> usp: ranging parameters:
[00:00:01.200,000] <inf> usp: bw: BW500
[00:00:01.200,000] <inf> usp: sf: SF9
[00:00:01.200,000] <inf> usp: rng_req_delay           =85
[00:00:05.847,000] <inf> usp: .......................................
[00:00:05.847,000] <inf> usp: Ranging result[00]  distance = 125m and rssi = -45
[00:00:05.847,000] <inf> usp: Ranging result[01]  distance = 127m and rssi = -46
[00:00:05.847,000] <inf> usp: Ranging result[02]  distance = 124m and rssi = -44
[00:00:05.847,000] <inf> usp: [... additional results ...]
[00:00:05.847,000] <inf> usp: Distance =126
[00:00:05.847,000] <inf> usp:  Distance Median computed: 126 m
```

### Subordinate device output

```
[00:00:00.036,000] <inf> usp: ===== ranging and frequency hopping example =====
[00:00:00.040,000] <inf> usp: Running in ranging subordinate mode
[00:00:00.045,000] <inf> usp: Ranging hopping demo started
[00:00:01.186,000] <inf> usp: Subordinate Ranging config rx done -> go to ack this config
[00:00:01.186,000] <inf> usp: Subordinate Ranging config rx done, payload size: 7
[00:00:01.200,000] <inf> usp: ranging parameters:
[00:00:01.200,000] <inf> usp: bw: BW500
[00:00:01.200,000] <inf> usp: sf: SF9
[00:00:01.200,000] <inf> usp: rng_req_delay           =85
[00:00:01.200,000] <inf> usp: starting ranging exchange
[00:00:05.847,000] <inf> usp: .......................................
[00:00:05.847,000] <inf> usp: Distance =0
```

## Troubleshooting

### Common Issues

1. **No Ranging Results**:
   - Check device positioning (line of sight recommended)
   - Verify both devices use same configuration
   - Ensure adequate distance (minimum ~1 meter)

2. **Timeout Errors**:
   - Check RF environment for interference
   - Verify antenna connections
   - Try different frequency or power settings

3. **Inconsistent Results**:
   - Ensure stable device positioning
   - Check for RF reflections or multipath
   - Consider environmental factors (weather, obstacles)

4. **OLED Display Issues**:
   - Verify I2C connections and address
   - Check display power supply
   - Ensure SSD1306 driver configuration

## Technical Notes

- **Timing Critical**: Preamble length and IQ settings affect ranging accuracy
- **Calibration**: Uses factory calibration tables for optimal performance
- **Range**: Typically accurate from 1m to several kilometers depending on conditions
- **Precision**: Can achieve sub-meter accuracy under ideal conditions
