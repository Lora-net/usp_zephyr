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

| Parameter                            | Default Value | Description                       |
|--------------------------------------|---------------|-----------------------------------|
| `CONFIG_MAIN_STACK_SIZE`             | `12288`       | Main thread stack size (12KB)     |
| `CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE` | `8192`        | System workqueue stack size       |
| `CONFIG_HEAP_MEM_POOL_SIZE`          | `8192`        | Heap memory pool size             |
| `CONFIG_LOG_BUFFER_SIZE`             | `4096`        | Logging buffer size               |
| `CONFIG_USP_FLRP`                    | `n`           | FLRC burst commands + 20KB buffer |

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

### FLRC features (disabled by default)

- Use the FLRC protocol (FLRP)
- Burst commands (`0xB0`–`0xB2`) and the 20KB payload buffer are compiled only when this option is enabled
- Requires a LR20XX radio (FLRC burst support)
- Set `CONFIG_USP_FLRP=y` in `prj.conf` to enable

## Compilation


**Build:**
```bash
west build --pristine --board xiao_nrf54l15/nrf54l15/cpuapp --shield semtech_loraplus_expansion_board --shield semtech_wio_lr2021 usp_zephyr/samples/usp/rac/hw_modem
```

**Build for FLRP:**
```bash
west build --pristine --board xiao_nrf54l15/nrf54l15/cpuapp --shield semtech_loraplus_expansion_board --shield semtech_wio_lr2021 usp_zephyr/samples/usp/rac/hw_modem -- -DDTC_OVERLAY_FILE=boards/xiao_nrf54l15_nrf54l15_cpuapp_flrp.overlay -DCONFIG_USP_FLRP=y

west build --pristine --board nucleo_l476rg/stm32l476xx --shield semtech_mbed_wio_interface --shield semtech_loraplus_expansion_board --shield semtech_wio_lr2021 usp_zephyr/samples/usp/rac/hw_modem -- -DDTC_OVERLAY_FILE=boards/nucleo_l476rg_flrp.overlay -DCONFIG_USP_FLRP=y
```

**Flash the firmware:**
```bash
west flash --runner pyocd
```


## Usage

### Using the software bridge (recommended)

This sample talks to the host over a single UART (`src/unified_uart.c`), with no bridge board or CMD/BUSY/EVENT control GPIOs required. `python_bridge.py` is the preferred way to communicate with it: it demultiplexes the UART into a trace channel and a data channel and exposes each over stdout/stderr, a TCP socket, a PTY, or a file.

The script lives in the bare-metal project, not in this Zephyr tree: `examples/main_examples/hw_modem/python_test/python_bridge.py`

#### 1. Parameters

  * device selection and configuration
    --port PORT: set the port used for the connection
    --baudrate BAUDRATE: set the baudrate of the connection
    --timeout TIMEOUT: set the timeout of the connection
  * output setup
    --trace-out SINK: set the destination(s) for the trace channel (repeatable)
    --data-out SINK: set the destination(s) for the data channel (repeatable)

The SINK can be any of:
- 'stdout' or 'stderr' (this process's streams)
- 'pty' (Unix only; allocates a PTY and prints its path, forwards raw bytes both ways)
- 'socket[:PORT]' (TCP server, bidirectional, default port 5551 if PORT is omitted)
- 'file:PATH' (writes raw bytes to PATH, truncated at startup)

#### 2.1 Usage as a module

You can import the following symbols from the script:
  * add_python_bridge_options: add the script options to a user-provided ArgumentParser
  * extract_python_bridge_config: extract a HostConfig from the parsed arguments
  * open_device: a context manager that takes a HostConfig, opens the configured device, exposes and display trace and data output

Example:
```python
from python_bridge import *

def main():
    parser = ArgumentParser(description=__doc__)
    add_python_bridge_options(parser)
    args = parser.parse_args()
    config = extract_python_bridge_config(args)

    with open_device(config) as device:
        device.write(b"...")
        reply = device.read()
        ...
```

Using the `with` syntax, the hub will live as long as the scope.

#### 2.2 Usage as a standalone

The script is intended to be invoked by the `uv` tool, that takes care of the dependencies. If `uv` is not installed, you can still invoke the script with `python3`, but you will need to manually install the dependencies. Typical use:


#### 3.1 Linux example

Terminal 1 (bridge running in the background):
```
$ uv run python_bridge.py --port /dev/ttyACM0 --trace-out stdout
Python bridge on /dev/ttyACM0 @ 115200
  trace -> stdout
  data  -> socket: 127.0.0.1:5552
Ctrl+C to stop.
```

Terminal 2 (application communicating with hw_modem):
```python
from python_bridge import *

def main():
    parser = ArgumentParser(description=__doc__)
    add_python_bridge_options(parser)
    args = parser.parse_args()
    config = extract_python_bridge_config(args)

    with open_device(config, attach=True) as device:
        device.write(b"...")
        reply = device.read()
        ...
```

Note the `attach = True` parameter of `open_device`, it allows to connect to an already running python_bridge (the hub keeps on living after the scope). This option is not compatible with `--data-out pty`.

#### 3.2 Windows example

The bridge runs the same way on Windows, with a few differences:

  * `--port` takes a COM port instead of a tty path, e.g. `--port COM5`. Check Device Manager > Ports (COM & LPT) for the assigned number.
  * `--trace-out pty` / `--data-out pty` are Unix only. Consider using the default option (TCP sockets).
  * `uv run python_bridge.py ...` works the same from PowerShell or Command Prompt.

Example (Command Prompt or PowerShell):
```
> uv run python_bridge.py --port COM5 --trace-out stdout
Python bridge on COM5 @ 115200
  trace -> stdout
  data  -> socket: 127.0.0.1:5552
Ctrl+C to stop.
```

### Using a physical bridge board

#### 1. Hardware Setup

1. **Connect Bridge Board**: Wire UART and GPIO signals through shield to external pins
2. **UART Connection**: Connect `smtc-hal-uart` (UART1) to bridge module
3. **GPIO Wiring**: Connect CMD, BUSY, EVENT GPIOs to bridge board
4. **Power Supply**: Ensure adequate power for modem operations

#### 2. Command Interface

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

#### 3. Python Test Suite

Refer to the README.md file present in the python_test directory

#### 4. Serial Command Example

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

### FLRC burst (requires `CONFIG_USP_FLRP=y`)
- `0xB0` - Start FLRC burst TX or RX mode (returns `radio_access_id`)
- `0xB1` - Set FLRC burst RX parameters (parameter `flrc_burst_params_pb_t`)
- `0xB2` - Get FLRC burst statistics (payload can be retrieved via NHM get results command)

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
- The NUCLEO-U575ZI-Q is not supported
