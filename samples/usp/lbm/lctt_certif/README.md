# LoRaWAN Certification (LCTT)

This application provides testing for the LoRaWAN Certification Test Tool (LCTT). Please see <https://resources.lora-alliance.org/home/getting-started-with-the-lorawan-certification-test-tool-lctt-2> for more information. It enables LoRa Basics Modem compliance testing by offering both normal operation and certification modes that can be toggled via button press.

## Key Features

- **Dual Operation Modes**: Normal periodic uplinks and LCTT certification mode
- **Button-Controlled Toggle**: Switch between modes using user button
- **Advanced LBM Features**: Class B/C, multicast, CSMA support for comprehensive testing
- **Persistent Mode**: Certification mode setting persists across reboots

## Configuration

### Network Credentials (Required)

Configure your LoRaWAN credentials in `boards/user_keys.overlay`:

```dts
/ {
    zephyr,user {
        user-lorawan-device-eui = <0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00>;
        user-lorawan-join-eui = <0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00>;
        user-lorawan-gen_app-key = <0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
                                   0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00>;
        user-lorawan-app-key = <0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
                               0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00>;
        user-lorawan-region = "EU_868";
    };
};
```

### LoRaWAN Regions

| Region    | Description           |
|-----------|-----------------------|
| `EU_868`  | Europe 868 MHz        |
| `US_915`  | North America 915 MHz |
| `AS_923`  | Asia-Pacific 923 MHz  |
| `AU_915`  | Australia 915 MHz     |
| `CN_470`  | China 470 MHz         |
| `IN_865`  | India 865 MHz         |

### Certification Requirements

- **LCTT Software**: LoRaWAN Certification Test Tool from LoRa Alliance
- **Gateway Connection**: Direct gateway link to LCTT Network Server
- **Isolated Environment**: Use only in isolated RF environments during certification

## Compilation

```bash
west build --pristine --board nucleo_l476rg/stm32l476xx --shield semtech_lr2021mb1xxs samples/usp/lbm/lctt_certif
```

## Usage

1. **Network Setup**: Configure LoRaWAN credentials (see `periodical_uplink` documentation)
2. **LCTT Setup**: Connect device to gateway linked with LCTT software
3. **Operation**:
   - **Normal Mode**: Automatic periodic uplinks every 10 seconds on port 101
   - **Button Press**: Toggles between normal and certification modes
   - **Certification Mode**: LCTT takes full control, bypassing duty cycle limits

## Expected Output

### Mode Toggle
```
[00:00:01.000,000] <inf> lctt_certif: Certification example is starting  
[00:00:01.100,000] <inf> lctt_certif: Push button to enable/disable certification
[00:00:30.200,000] <inf> lctt_certif: Button pushed
[00:00:30.300,000] <inf> lctt_certif: Certification mode enabled
```

### Normal Operation
```
[00:00:02.200,000] <inf> lctt_certif: Event received: JOINED
[00:00:12.300,000] <inf> lctt_certif: Event received: ALARM  
[00:00:12.400,000] <inf> lctt_certif: Event received: TXDONE
```

## Important Notes

- **Certification Mode**: Should only be used in isolated RF environments for compliance testing
- **Mode Persistence**: Certification mode setting survives device resets
- **LCTT Control**: In certification mode, the LCTT software controls all device operations
- **Compliance Testing**: Used to verify LoRaWAN specification conformance
