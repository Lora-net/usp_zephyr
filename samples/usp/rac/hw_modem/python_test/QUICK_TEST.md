# Quick Test Guide

## Testing the Implementation

### 1. Test Syntax
```bash
python3 -c "import rac_modem_api; print('✅ API OK')"
python3 -c "import test_ping_pong; print('✅ Ping Pong OK')"  
```

### 2. Test Basic Modem Connection
```bash
python3 test_hw_modem.py --baudrate 115200 /dev/ttyUSB0
```

### 3. Test Ping Pong Protocol
```bash
# Terminal 1: Python manager
python3 test_ping_pong.py /dev/ttyUSB0 --baudrate 115200 --mode manager

# Terminal 2: Python subordinate (or native Zephyr)  
python3 test_ping_pong.py /dev/ttyUSB1 --baudrate 115200 --mode subordinate
```

## Expected Output

### Manager:
```
👑 Manager cycle 1
📤 Sending PING (counter=0, delay=2500ms)  
   ✅ PING sent successfully
📥 Waiting for RX (timeout=500ms)
   🔍 Polling for RX results...
   ✅ Received PONG (counter=0)
   📊 RSSI: -45 dBm, SNR: 12 dB
```

### Subordinate:  
```
🤖 Subordinate cycle
📥 Waiting for RX (timeout=30000ms)
   🔍 Polling for RX results...
   ✅ Received PING (counter=0)
   📊 RSSI: -47 dBm, SNR: 11 dB
📤 Sending PONG (counter=0, delay=10ms)
   ✅ PONG sent successfully
```

## Cross-Platform Compatibility

✅ **Works with native Zephyr ping_pong example**
- Same radio parameters (868.1 MHz, SF9, BW125, CR4/5)
- Same protocol (PING/PONG + counter + timeouts)  
- Same payload format (4+1+1 bytes)
- Compatible timing (500ms/30s timeouts)
