# Hardware Modem

This application implements a **complete LoRa Basics Modem hardware interface** that exposes all LBM functionality through UART communication and GPIO control signals. It transforms the device into a standalone modem that can be controlled by external host systems via a standardized command protocol, making it ideal for integrating LoRaWAN capabilities into existing products without direct LBM integration.

## Key Features

- **Complete LBM API Access**: All LoRa Basics Modem functions accessible via commands
- **UART + GPIO Interface**: Standard communication using RX/TX + 3 control GPIOs (CMD, BUSY, EVENT)
- **Bridge Compatible**: Designed for use with bridge boards for host communication
- **Protobuf Serialization**: Complex data structures handled via Protocol Buffers
- **Dual Protocol Support**: Legacy protocol and NHM (New Hardware Modem) protocol
- **Command Validation**: CRC-protected command interface with proper error handling
- **Low Power Support**: Sleep mode management with interrupt-driven wake-up

## Configuration

### Using CMake

| Parameter                            | Default Value | Description                   |
|--------------------------------------|---------------|-------------------------------|
| `CONFIG_MAIN_STACK_SIZE`             | `12288`       | Main thread stack size (12KB) |
| `CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE` | `8192`        | System workqueue stack size   |
| `CONFIG_HEAP_MEM_POOL_SIZE`          | `8192`        | Heap memory pool size         |
| `CONFIG_LOG_BUFFER_SIZE`             | `4096`        | Logging buffer size           |

### GPIO Configuration (Device Tree)

```dts
smtc-hal-uart = &uart1;
hw-modem-command-gpios = <&arduino_header 8 GPIO_ACTIVE_HIGH>;
hw-modem-busy-gpios = <&arduino_header 9 GPIO_ACTIVE_HIGH>;
hw-modem-event-gpios = <&arduino_header 10 GPIO_ACTIVE_HIGH>;
hw-modem-led-scan-gpios = <&arduino_header 11 GPIO_ACTIVE_HIGH>;
```

### LBM Features (Enabled)

- LoRaWAN Class B and Class C support
- Multicast and CSMA capabilities
- FUOTA (Firmware Update Over The Air)
- Application Layer Clock Synchronization (ALC Sync v1/v2)
- Geolocation and almanac services
- Stream and Large File Upload (LFU)
- Device management and store-and-forward
- Relay TX/RX functionality

## Compilation

### USP Zephyr

**Build:**
```bash
west build --pristine --board xiao_nrf54l15/nrf54l15/cpuapp --shield semtech_loraplus_expansion_board --shield semtech_wio_lr2021 usp_zephyr/samples/usp/rac/hw_modem
```

**Flash the firmware:**
```bash
west flash
```

### USP 
**Build sample:**
- hw_modem for lr2021 (no geolocation)
```
rm -Rf build/ ; cmake -L -S examples  -B build -DAPP=HW_MODEM -DCMAKE_BUILD_TYPE=MinSizeRel -DBOARD=NUCLEO_L476 -DRAC_RADIO=lr2021 -DLBM_GEOLOCATION=OFF -G Ninja; cmake --build build --target hw_modem
```

- hw_modem for lr1120
```
rm -Rf build/ ; cmake -L -S examples  -B build -DAPP=HW_MODEM -DCMAKE_BUILD_TYPE=MinSizeRel -DBOARD=NUCLEO_L476 -DRAC_RADIO=lr2021 -G Ninja; cmake --build build --target hw_modem
```

- hw_modem for lr1120 with LBM_CRYPTO=LR11XX
```
rm -Rf build/ ; cmake -L -S examples  -B build -DAPP=HW_MODEM -DCMAKE_BUILD_TYPE=MinSizeRel -DBOARD=NUCLEO_L476 -DRAC_RADIO=lr1120 -DLBM_RELAY_TX=OFF -DLBM_RELAY_RX=OFF -DLBM_CRYPTO=LR11XX -G Ninja; cmake --build build --target hw_modem
```

**Example of `openocd`command to flash:**
```bash
openocd -f interface/stlink.cfg -f target/stm32l4x.cfg -c "adapter serial <SERIAL_NUMBER>" -c "program build/hw_modem verify reset exit"
```


## Usage

### 1. Hardware Setup

1. **Connect Bridge Board**: Wire UART and GPIO signals through shield to external pins
2. **UART Connection**: Connect `smtc-hal-uart` (UART1) to bridge module
3. **GPIO Wiring**: Connect CMD, BUSY, EVENT GPIOs to bridge board
4. **Power Supply**: Ensure adequate power for modem operations

### 2. Command Interface

Commands follow the format:
```
<command_id> <command_size> <command_data> <crc>
```

**Example - Get Version (0x10):**
```
0x10 0x00 0x10
```

**Example - Open RAC Session (0xA2):**
```
0xA2 0x01 0x01 0xA0
```

### 3. Python Test Suite

Refer to the README.md file present in the python_test directory

### 4. Serial Command Example

**Using Python:**
```python
import serial
ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=0.5)

# Get version command
ser.write(b'\x10\x00\x10')
response = ser.read(100)
```

## Expected Output

### Initialization
```
[00:00:01.000,000] <inf> hw_modem: Modem is starting
[00:00:01.100,000] <dbg> hw_modem: Commit SHA1: a1b2c3d4
[00:00:01.100,000] <dbg> hw_modem: Build date: 2024-01-15
```

### Command Processing
```
[00:00:05.200,000] <inf> hw_modem: Command received: 0x10 (Get Version)
[00:00:05.250,000] <inf> hw_modem: Response sent: version 040900
[00:00:06.100,000] <inf> hw_modem: Command received: 0xA2 (RAC Open)
[00:00:06.150,000] <inf> hw_modem: RAC session opened with handle 0x01
```

## Available Commands

### Core Commands
- `0x10` - Get LBM Version
- `0x5D` - Get Device Information
- `0xA0` - Submit USP Transaction (Legacy)
- `0xA2` - Open RAC Session
- `0xA3` - Close RAC Session
- `0xA5` - Get RAC Results (Legacy)
- `0xA6` - NHM Extended Command

### LoRaWAN Commands
- `0x41` - Join Network
- `0x42` - Send Uplink
- `0x43` - Request Downlink
- `0x44` - Set Device Class
- `0x45` - Configure Multicast

### Advanced Features
- FUOTA commands (0x60-0x6F)
- Geolocation commands (0x70-0x7F)
- Clock sync commands (0x80-0x8F)
- Device management commands (0x90-0x9F)

## Technical Notes

- **Protocol Buffers**: Complex data serialization for RAC context and results
- **Segmentation**: Automatic for payloads exceeding 251 bytes (NHM protocol)
- **CRC Protection**: All commands include CRC validation
- **Bridge Interface**: GPIO signals routed through shield connector

## Integration Notes

- **Host Requirements**: Serial interface + 3 GPIO control lines
- **Bridge Board**: Required for connecting to host systems

## Limitations

- On NUCLEO-STM32L476RG, due to the USART, the power mode is deactivated