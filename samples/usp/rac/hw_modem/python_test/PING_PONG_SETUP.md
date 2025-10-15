# Ping-Pong Protocol Test Setup

## Overview

This directory contains `test_ping_pong.py`, a Python implementation of the ping-pong protocol that communicates with a C subordinate device running the native ping-pong firmware.

## Hardware Setup

You need **TWO devices** for a complete ping-pong test:

1. **Python Manager**: Computer running `test_ping_pong.py`
2. **C Subordinate**: Hardware device with compiled ping-pong firmware

## C Subordinate Compilation

The Python timing parameters are synchronized with a **5-second processing delay** in the C subordinate. You MUST compile the C subordinate with this specific configuration:

```bash
west build --pristine --board nucleo_l476rg/stm32l476xx --shield semtech_lr1120mb1xxs usp_zephyr/samples/usp/sdk/ping_pong/ -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_C_FLAGS="-DSUBORDINATE_PROCESSING_TIME_MS=5000"
```

Then flash to the subordinate device:
```bash
west flash
```

## Python Manager Usage

```bash
# Start as manager (sends first PING)
python test_ping_pong.py /dev/ttyUSB0 --mode manager

# Or start as subordinate (waits for PING) - for testing with another Python instance
python test_ping_pong.py /dev/ttyUSB0 --mode subordinate
```

## Protocol Flow

```
Manager (Python)         Subordinate (C Firmware)
     |                           |
     |-- PING + counter -------->|
     |                           | (5s processing delay)
     |<------ PONG + counter ----|
     |                           |
     |-- PING + counter+1 ------>|
     |                           | (5s processing delay)
     |<------ PONG + counter+1 --|
     |                           |
    ...                         ...
```

## Timing Parameters

The Python script uses these timing parameters to match the 5-second subordinate delay:

- `DELAY_BETWEEN_TX = 3000ms`: Delay between TX transactions
- `MANAGER_RX_TIMEOUT = 18000ms`: RX timeout (5s + 13s margin)
- `SUBORDINATE_RX_TIMEOUT = 30000ms`: RX timeout for subordinate mode
- `INTER_CYCLE_DELAY = 1000ms`: Delay between ping-pong cycles

## Features

- **NHM Protocol Support**: Automatic segmentation for large payloads
- **Strict Validation**: Matches C application payload validation logic
- **Error Handling**: Consecutive failure counting with automatic retry
- **Automatic Restart**: Switches to subordinate mode after max exchanges/failures
- **Real-time Debugging**: Shows payload hex/ASCII and radio parameters

## Troubleshooting

1. **Timeout errors**: Ensure C subordinate is compiled with `-DSUBORDINATE_PROCESSING_TIME_MS=5000`
2. **Payload validation errors**: Check that both devices use the same radio configuration
3. **Serial connection issues**: Verify correct port and baud rate (115200)
4. **Protobuf errors**: Run `./build.sh` in the serialization directory

## Example Output

```
👑 Manager cycle 1
📤 Sending PING (counter=0, delay=3000ms)
   ✅ PING command accepted
   🔍 Waiting for TX completion...
   ✅ TX DONE - PING transmission completed successfully
📥 Starting RX (timeout=18000ms)
   🔍 Polling for RX results...
   ✅ RX DONE - Received PONG (counter=0)
   📊 RSSI: -45 dBm, SNR: 12 dB
   ✅ payload is correct, continuing (PONG counter=0)
```
