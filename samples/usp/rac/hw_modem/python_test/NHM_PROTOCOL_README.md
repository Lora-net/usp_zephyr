# NHM (New Hw Modem) Protocol Testing

## Overview

The NHM protocol extends the existing hw_modem protocol with automatic segmentation support, allowing payloads larger than 255 bytes to be sent and received seamlessly.

## Key Features

### ✅ **Automatic Segmentation**
- Payloads up to **700 bytes** supported
- Transparent segmentation/reassembly
- UCI-compatible header format

### ✅ **Backward Compatibility**
- Legacy commands (0x00-0xA5) unchanged
- NHM commands use new 0xA6 entry point
- Both protocols can coexist

### ✅ **Extended Addressing** 
- **4096 possible commands** (12-bit addressing)
- Future support for responses and notifications

## Protocol Format

```
NHM Header (4 bytes):
┌─────────────┬─────────────┬─────────────┬─────────────┐
│ MT+PBF+ID_H │   ID_LOW    │     RFU     │   LENGTH    │
└─────────────┴─────────────┴─────────────┴─────────────┘
     Byte 0        Byte 1       Byte 2       Byte 3

• MT[7:5]   : Message Type (1=Command, 2=Response, 3=Notification)
• PBF[4]    : Packet Boundary Flag (0=Complete/Last, 1=Intermediate)
• ID[11:0]  : Command ID (12 bits = 4096 commands)
• RFU       : Reserved for Future Use
• LENGTH    : Payload length (0-251 bytes per packet)
```

## Testing Examples

### 1. Basic Comparison Test
```bash
# Test both legacy and NHM protocols
python test_hw_modem.py /dev/ttyUSB0 --mode comparison
```

This will:
- Test legacy protocol with small payload (~42 bytes)
- Test NHM protocol with maximum payload (255 bytes)
- Show automatic segmentation in action

### 2. Detailed Segmentation Test
```bash
# Test various payload sizes with NHM
python test_hw_modem.py /dev/ttyUSB0 --mode nhm_test
```

This will test:
- 50 bytes (no segmentation)
- 251 bytes (maximum single packet)
- 300 bytes (2 segments)
- 500 bytes (2 segments)
- 700 bytes (3 segments, maximum)

### 3. Interactive Mode
```bash
# Manual command testing
python test_hw_modem.py /dev/ttyUSB0 --mode interactive
```

Available commands:
- **7**: NHM RAC LoRa (with custom payload)
- **8**: NHM Get Results (with polling)
- **9**: Run comparison test
- **10**: Run segmentation test

## API Usage Examples

### Legacy Protocol (< 251 bytes)
```python
from rac_modem_api import RacModemAPI

api = RacModemAPI('/dev/ttyUSB0')
api.connect()

# Small payload - uses legacy protocol
result = api.rac_lora({"data": {"payload": "Small test"}})
```

### NHM Protocol (any size)
```python
# Large payload - automatic segmentation
large_payload = "X" * 400  # 400 bytes
result = api.nhm_rac_lora({"data": {"payload": large_payload}})

# Get results via NHM (can handle large responses)  
results = api.nhm_rac_get_results(max_attempts=15)
```

## Expected Behavior

### Legacy Protocol
- **Limit**: ~255 bytes total message size
- **Commands**: 0xA0 (rac_lora), 0xA5 (get_results)
- **Segmentation**: None
- **Use case**: Small payloads, existing compatibility

### NHM Protocol  
- **Limit**: 700 bytes reassembled message
- **Commands**: 0xA6 (nhm_extended) → 0x100 (rac_lora), 0x102 (get_results)
- **Segmentation**: Automatic (251 bytes per segment)
- **Use case**: Large payloads, future extensibility

## Troubleshooting

### Common Issues

1. **"NHM: Payload requires segmentation"** - Normal for payloads > 251 bytes
2. **"Parse error"** - Check that firmware supports NHM protocol (CMD_NHM_EXTENDED)
3. **"Segmentation timeout"** - Increase polling attempts or check serial connection

### Debug Information

The test script shows detailed information:
- Payload sizes and segmentation details
- Protocol used (Legacy vs NHM)
- Segment transmission progress
- Response parsing status

## Next Steps

1. **Test Basic Functionality**: Run comparison mode first
2. **Verify Segmentation**: Test with increasing payload sizes
3. **Performance Testing**: Measure latency with large payloads
4. **Integration**: Use in your own applications

## Implementation Status

- ✅ Firmware: NHM protocol implemented
- ✅ Python: Wrapper with automatic segmentation  
- ✅ Tests: Comprehensive test suite
- 🔄 Future: Notifications, responses, extended commands
