# RAC Modem Python Test Suite

## 📋 Overview

This directory contains a modular Python test suite for the RAC modem, providing clean APIs for all modem commands and comprehensive testing capabilities.

## 📁 File Structure

```
python_test/
├── rac_modem_api.py            # 🔧 Modular API library
├── test_hw_modem.py            # 🧪 Unified test script
├── test_ping_pong.py           # 🏓 Ping Pong protocol implementation
├── test_hw_modem_backup.py     # 📋 Backup of original test script
└── README.md                   # 📖 This documentation
```

## 🔧 RacModemAPI Library

The `rac_modem_api.py` provides a clean, modular interface for all RAC modem commands:

### **Supported Commands:**
- ✅ `get_version()` - Get LBM version (CMD 0x10)
- ✅ `rac_open()` - Open RAC session (CMD 0xA2)
- ✅ `rac_lora()` - Execute LoRa transaction (CMD_USP_SUBMIT 0xA0)
- ✅ `rac_lora_tx()` - Execute LoRa TX transaction (CMD_USP_SUBMIT 0xA0)
- ✅ `rac_lora_rx()` - Execute LoRa RX transaction (CMD_USP_SUBMIT 0xA0)
- ✅ `rac_get_results()` - Poll for results (CMD 0xA5)
- ✅ `rac_close()` - Close RAC session (CMD 0xA3)

### **Key Features:**
- **Automatic connection management** with proper cleanup
- **Protobuf integration** for complex data structures
- **Intelligent result polling** with configurable timeouts
- **Pretty-printed results** with error handling
- **Flexible configuration** via dictionary parameters

## 🧪 Test Script Usage

The unified `test_hw_modem.py` script supports two testing modes:

### **1. Complete Workflow (Default)**
```bash
# Run full workflow: version → open → lora → get_results → close
# Results polling is automatically integrated after rac_lora
python test_hw_modem.py /dev/ttyUSB0
```

### **2. Interactive Mode**
```bash
# Interactive command-by-command testing
python test_hw_modem.py /dev/ttyUSB0 --mode interactive
```

### **3. Advanced Options**
```bash
# Custom baudrate and timeout
python test_hw_modem.py /dev/ttyACM0 --baudrate 9600 --timeout 2.0

# All available options
python test_hw_modem.py --help
```

## 🏓 Ping Pong Protocol

The `test_ping_pong.py` script implements the full ping-pong protocol, compatible with the native Zephyr ping_pong example.

### **Usage:**
```bash
# Start as manager (sends first PING)
python test_ping_pong.py /dev/ttyUSB0 --mode manager

# Start as subordinate (waits for PING, responds with PONG)
python test_ping_pong.py /dev/ttyUSB0 --mode subordinate

# Custom serial settings
python test_ping_pong.py /dev/ttyACM0 --mode manager --baudrate 9600
```

### **Protocol Features:**
- 📡 **Manager**: Sends PING + counter, waits for PONG with same counter
- 🤖 **Subordinate**: Waits for PING, responds with PONG using received counter
- 🔄 **Auto-restart**: After 25 exchanges or 5 consecutive failures
- ⏱️ **Timeouts**: 500ms (manager) / 30s (subordinate)
- 📊 **Compatible**: Works with native Zephyr ping_pong project

### **Cross-Platform Testing:**
```bash
# Side 1: Python ping-pong manager
python test_ping_pong.py /dev/ttyUSB0 --mode manager

# Side 2: Native Zephyr ping_pong subordinate
cd /path/to/zephyr/samples/usp/sdk/ping_pong
west build -b your_board
west flash
```

## 🔌 API Usage Examples

### **Basic Usage**
```python
from rac_modem_api import RacModemAPI

# Initialize and connect
api = RacModemAPI('/dev/ttyUSB0')
if api.connect():

    # Get version
    result = api.get_version()
    api.print_result(result, "Version Check")

    # Open session
    result = api.rac_open()
    if result.get("success"):
        print(f"Radio handle: 0x{result['radio_handle']:02x}")

    # Execute LoRa transaction
    config = {
        "data": {"payload": "Hello World!"},
        "radio": {"frequency_hz": 868100000, "tx_power_dbm": 14}
    }
    result = api.rac_lora(config)

    # Poll for results
    if result.get("success"):
        results = api.rac_get_results(max_attempts=10, poll_interval=1.0)
        api.print_result(results, "Results")

    # Close session
    api.rac_close()
    api.disconnect()
```

### **TX/RX Operations**
```python
# Direct TX operation
payload = b"Hello LoRa World!"
tx_config = {
    "radio": {"frequency_hz": 868100000, "tx_power_dbm": 14}
}
result = api.rac_lora_tx(payload, tx_config)

# Direct RX operation with timeout
rx_config = {
    "radio": {"frequency_hz": 868100000}
}
result = api.rac_lora_rx(rx_timeout_ms=10000, context_config=rx_config)

# Poll for RX results
if result.get("success"):
    results = api.rac_get_results()
    if results.get("parsed_results", {}).get("transaction_status") == 1:  # COMPLETED
        received = results["parsed_results"]["payload_data"]
        print(f"Received: {received}")
```

### **Advanced Configuration**
```python
# Custom RAC context
context_config = {
    "radio": {
        "is_tx": True,
        "frequency_hz": 868300000,
        "tx_power_dbm": 20,
        "crc_enabled": True
    },
    "data": {
        "payload": b"Binary payload data"
    },
    "scheduler": {
        "start_time_ms": 15000
    }
}

result = api.rac_lora(context_config)
```

## 📊 Result Structure

All API methods return structured dictionaries:

### **Common Fields:**
```python
{
    "success": bool,           # True if command succeeded
    "return_code": int,        # Modem return code
    "crc_received": int,       # Response CRC
    "payload_length": int,     # Response payload length
    "payload": bytes,          # Raw response payload
    "raw": str                 # Full hex response
}
```

### **Command-Specific Fields:**

**`get_version()`:**
```python
{"version": "040900"}  # LBM version in hex
```

**`rac_open()`:**
```python
{"radio_handle": 0x12}  # Assigned radio handle
```

**`rac_lora()`:**
```python
{"context_size": 45}  # Serialized context size
```

**`rac_get_results()`:**
```python
{
    "parsed_results": {
        "transaction_status": 1,    # Status code
        # results available when transaction_status == 1
        "rssi_result": -85,         # RSSI in dBm
        "snr_result": 12,           # SNR in dB
        "payload_size": 21,         # Payload size
        "payload_data": b"...",     # Received payload
        "radio_start_timestamp_ms": 12345,
        "radio_end_timestamp_ms": 12567,
        "attempt": 3                # Polling attempt number
    }
}
```

## ⚠️ Requirements

- **Python 3.6+** with `pyserial` package
- **Protobuf support**: Run `./build.sh` in `../serialization/` directory first
- **Hardware**: Connected RAC modem via serial port

## 🔧 Troubleshooting

### **"Protobuf not available"**
```bash
cd ../serialization/
./build.sh
```

### **"Serial connection failed"**
- Check port permissions: `sudo chmod 666 /dev/ttyUSB0`
- Verify correct port: `ls /dev/tty*`
- Check if port is already in use

### **"Response too short" or "Timeout"**
- Verify modem is powered and connected
- Try different baudrate: `--baudrate 9600`
- Increase timeout: `--timeout 5.0`
