# Quick Test Guide - NHM Protocol

## 🚀 Ready to Test!

The NHM (New Hw Modem) protocol implementation is complete and ready for testing.

## Default Test Sequence

When you run the test script, it automatically executes this complete sequence:

```bash
python3 test_hw_modem.py /dev/ttyUSB0
```

### What happens:

1. **📋 Get LBM Version** - Check modem connectivity
2. **🔓 Open RAC Session** - Initialize radio (ID: 0x01)  
3. **📡 Small Payload Test** (~33 bytes)
   - Uses **Legacy Protocol** (CMD_USP_SUBMIT = 0xA0)
   - Payload: "Hello RAC Small Payload Test!"
   - **🔍 Get Results** via legacy protocol
4. **📡 Maximum Payload Test** (255 bytes)
   - Uses **NHM Protocol** (CMD_NHM_EXTENDED = 0xA6)
   - Payload: 255 x "X" characters
   - **🔀 Automatic segmentation** (2 segments: 251 + 4 bytes)
   - **🔍 Get Results** via NHM protocol
5. **🔒 Close RAC Session**

## Expected Output

```
🧪 Running Complete RAC Workflow - Legacy + NHM Protocol Testing
======================================================================

📋 Test 1: Get LBM Version
✅ Success: Modem version retrieved

🔓 Test 2: Open RAC Session  
✅ Success: Radio opened with handle 0x01

📡 Test 3: RAC LoRa Transaction - Small Payload (Legacy Protocol)
----------------------------------------------------------------------
🔍 TX: a0210a08011240... (33 bytes payload)
✅ Success: Legacy protocol transaction

🔍 Test 4: Get Results - Small Payload
✅ Success: Results retrieved via legacy protocol

📡 Test 5: RAC LoRa Transaction - Maximum Payload (NHM Protocol)  
----------------------------------------------------------------------
🔀 Using NHM protocol for large payload (255 bytes)
🔀 NHM: Payload requires segmentation (440 bytes > 251 bytes)
🔍 TX: a6ff... (segment 1/2)
🔍 TX: a6... (segment 2/2)  
✅ Success: NHM protocol with segmentation

🔍 Test 6: Get Results - Maximum Payload
✅ Success: Results retrieved via NHM protocol

🔒 Test 7: Close RAC Session
✅ Success: Radio session closed

✅ Complete workflow finished - Both protocols tested
📊 Summary:
   • Legacy Protocol: Small payload (~33 bytes) via CMD_USP_SUBMIT (0xA0)
   • NHM Protocol: Maximum payload (255 bytes) via CMD_NHM_EXTENDED (0xA6)
   • Segmentation: Automatic for payloads > 251 bytes
```

## Interactive Mode

For manual testing:

```bash
python3 test_hw_modem.py /dev/ttyUSB0 --mode interactive
```

Available options:
- **6**: Complete Workflow (runs the full sequence above)
- **7**: NHM Custom Payload Test (with size selection)
- **8**: NHM Get Results

## Troubleshooting

### Before Testing:
1. **Compile firmware** with NHM modifications
2. **Build protobuf** files: `cd ../serialization && ./build.sh`
3. **Connect hardware** to specified serial port

### Common Issues:
- **"Protobuf not available"** → Run `build.sh` in serialization directory
- **"CMD_NHM_EXTENDED: Unknown command"** → Firmware not updated
- **"Segmentation timeout"** → Check serial connection/firmware logs

## Key Differences

| Protocol | Command | Max Size | Segmentation | Use Case |
|----------|---------|----------|-------------|----------|
| **Legacy** | 0xA0/0xA5 | ~255 bytes | None | Small payloads, compatibility |
| **NHM** | 0xA6→0x100/0x102 | 700 bytes | Automatic | Large payloads, future features |

## Success Indicators

✅ **Both protocols work** → Ready for production
✅ **Segmentation transparent** → Large payloads handled automatically  
✅ **Backward compatibility** → Legacy systems unaffected

The test validates that your NHM protocol implementation correctly handles both small payloads (legacy path) and large payloads (NHM path with segmentation).
