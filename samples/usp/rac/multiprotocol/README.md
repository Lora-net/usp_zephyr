# Multiprotocol

This sample joins a LoRa network and periodically sends empty plinks.
It also make it possible to configure the ranging test role (manager or subordinate) and the ranging test priority via the shell commands. It starts a ranging test whenever the USER button is pressed or triggered by the shell.
The ranging test result is sent over LoraWan by the manager to the network server.

## Key Features

- **LoRaWAN Integration**: Periodic uplinks every 60 seconds (same as `periodical_uplink` sample)
- **Button-Triggered Ranging**: LoRa ranging tests initiated by button press (same as `ranging_demo` sample)
- **Multi-Protocol Management**: Coordinated access between LoRaWAN and ranging operations
- **Network Credentials**: Device provisioning via device tree overlays
- **shell integration**: Allow to configure and trigger actions

## Requirements

* Two boards with a LoRa transceiver supported by Zephyr.
* A LoRaWAN network
* Two uart consoles to interact with the boards

## Main shell commands

The following shell commands are available:
- mode <manager|subordinate> <VERY_HIGH | HIGH | MEDIUM| LOW | VERY_LOW>: set the ranging test role and priority. Very high, high and medium are above LoRaWAN uplink messages.
- ranging <start|info>: start a ranging test (only on manager) or get ranging result information
- req_time: send a request lorawan mac command to get the current time from the network server
- time: display the current time
- button: simulate a USER button press to start a ranging test (only on manager)
- uplink: send an uplink immediately
- status: display the current status of the application
- help: show all available commands

## Usage

1. **Start devices**: Start 2 devices. Normally, each device starts the LoRaWAN join sequence and, after joining, sends a payload every 60 seconds.

2. **Prepare ranging setup**: Configure the mode on each device, one device is set as subordinate and the other as manager, using the mode command.

```
mode manager VERY_LOW
mode subordinate VERY_LOW
```

3. **Start first ranging**: On the manager, enter the command: `ranging start`. After the first ranging event is started.

4. **Start other ranging sequence**: To start the next ranging sequence, press the user button.

## Expected Output

In Manager mode
```
multiprotocol:~$ ranging start
Starting ranging exchange...
multiprotocol:~$ INFO: CONF Manager Ranging exchange with single data rate enabled
INFO: Manager Ranging config tx done  -> go to rx wait for subordinate answer
INFO: starting ranging exchange
[1980-01-06 00:00:44.288] <inf> multiprotocol: Launch ranging
multiprotocol:~$ .............................
INFO: Ranging result[00] freq = 863750000, distance = -1m, rssi = -25
INFO: Ranging result[01] freq = 865100000, distance = 0m, rssi = -26
INFO: Ranging result[02] freq = 864800000, distance = 0m, rssi = -26
INFO: Ranging result[03] freq = 868400000, distance = -1m, rssi = -27
INFO: Ranging result[04] freq = 865250000, distance = 0m, rssi = -26
INFO: Ranging result[05] freq = 867500000, distance = 0m, rssi = -27
INFO: Ranging result[06] freq = 865550000, distance = 0m, rssi = -26
INFO: Ranging result[07] freq = 867650000, distance = 0m, rssi = -27
INFO: Ranging result[08] freq = 866150000, distance = 0m, rssi = -26
INFO: Ranging result[09] freq = 864050000, distance = 0m, rssi = -25
INFO: Ranging result[10] freq = 864500000, distance = 0m, rssi = -26
INFO: Ranging result[11] freq = 866450000, distance = 0m, rssi = -26
INFO: Ranging result[12] freq = 865400000, distance = 0m, rssi = -26
INFO: Ranging result[13] freq = 868700000, distance = 0m, rssi = -27
INFO: Ranging result[14] freq = 863150000, distance = 0m, rssi = -25
INFO: Ranging result[15] freq = 866750000, distance = 0m, rssi = -26
INFO: Ranging result[16] freq = 866300000, distance = 0m, rssi = -26
INFO: Ranging result[17] freq = 864950000, distance = 0m, rssi = -26
INFO: Ranging result[18] freq = 864350000, distance = 0m, rssi = -25
INFO: Ranging result[19] freq = 866000000, distance = 0m, rssi = -26
INFO: Ranging result[20] freq = 866900000, distance = 0m, rssi = -26
INFO: Ranging result[21] freq = 868250000, distance = -1m, rssi = -27
INFO: Ranging result[22] freq = 865850000, distance = 0m, rssi = -26
INFO: Ranging result[23] freq = 865700000, distance = 0m, rssi = -26
INFO: Ranging result[24] freq = 867350000, distance = -1m, rssi = -26
INFO: Ranging result[25] freq = 868100000, distance = 0m, rssi = -27
INFO: Ranging result[26] freq = 863600000, distance = 0m, rssi = -25
INFO: Ranging result[27] freq = 866600000, distance = 0m, rssi = -26
INFO: Ranging result[28] freq = 864200000, distance = 0m, rssi = -25
[1980-01-06 00:00:47.099] <inf> multiprotocol: Ranging result: distance=0 m, SF=9, BW=11 kHz
```
After button pressed
```
multiprotocol:~$ INFO: CONF Manager Ranging exchange with single data rate enabled
INFO: Manager Ranging config tx done  -> go to rx wait for subordinate answer
INFO: starting ranging exchange
button_pressed[1980-01-06 00:00:56.597] <inf> multiprotocol: Button pushed
[1980-01-06 00:00:56.597] <inf> multiprotocol: Button pressed
multiprotocol:~$ .............................
INFO: Ranging result[00] freq = 863750000, distance = -3m, rssi = -28
INFO: Ranging result[01] freq = 865100000, distance = 0m, rssi = -28
INFO: Ranging result[02] freq = 864800000, distance = 0m, rssi = -28
INFO: Ranging result[03] freq = 868400000, distance = 0m, rssi = -25
INFO: Ranging result[04] freq = 865250000, distance = 0m, rssi = -25
INFO: Ranging result[05] freq = 867500000, distance = 0m, rssi = -25
INFO: Ranging result[06] freq = 865550000, distance = 0m, rssi = -25
INFO: Ranging result[07] freq = 867650000, distance = 0m, rssi = -26
INFO: Ranging result[08] freq = 866150000, distance = 0m, rssi = -26
INFO: Ranging result[09] freq = 864050000, distance = 0m, rssi = -26
INFO: Ranging result[10] freq = 864500000, distance = 0m, rssi = -26
INFO: Ranging result[11] freq = 866450000, distance = 0m, rssi = -26
INFO: Ranging result[12] freq = 865400000, distance = 0m, rssi = -26
INFO: Ranging result[13] freq = 868700000, distance = 0m, rssi = -27
INFO: Ranging result[14] freq = 863150000, distance = -1m, rssi = -26
INFO: Ranging result[15] freq = 866750000, distance = 0m, rssi = -27
INFO: Ranging result[16] freq = 866300000, distance = 0m, rssi = -26
INFO: Ranging result[17] freq = 864950000, distance = 0m, rssi = -26
INFO: Ranging result[18] freq = 864350000, distance = 0m, rssi = -26
INFO: Ranging result[19] freq = 866000000, distance = 0m, rssi = -26
INFO: Ranging result[20] freq = 866900000, distance = 0m, rssi = -27
INFO: Ranging result[21] freq = 868250000, distance = 0m, rssi = -27
INFO: Ranging result[22] freq = 865850000, distance = 0m, rssi = -26
INFO: Ranging result[23] freq = 865700000, distance = 0m, rssi = -26
INFO: Ranging result[24] freq = 867350000, distance = -1m, rssi = -27
INFO: Ranging result[25] freq = 868100000, distance = 0m, rssi = -27
INFO: Ranging result[26] freq = 863600000, distance = 0m, rssi = -26
INFO: Ranging result[27] freq = 866600000, distance = 0m, rssi = -26
INFO: Ranging result[28] freq = 864200000, distance = 0m, rssi = -26
[1980-01-06 00:00:59.404] <inf> multiprotocol: Ranging result: distance=0 m, SF=9, BW=11 kHz
```

In Subordinate mode
```
multiprotocol:~$ mode subordinate LOW
Device set as SUBORDINATE
Ranging priority set to LOW
multiprotocol:~$ INFO: Ranging hopping demo started
multiprotocol:~$ INFO: RX Subordinate Ranging config rx done -> go to ack this config
INFO: RX Subordinate Ranging config rx done, payload size: 7
INFO: ranging parameters:
INFO: bw: RAL_LORA_BW_500_KHZ
INFO: sf: RAL_LORA_SF9
INFO: rng_req_delay           =85
INFO: starting ranging exchange
............................
[2026-02-20 16:00:44.527] <inf> multiprotocol: Ranging result: distance=0 m, SF=9, BW=11 kHz
```

## Compilation

You first need to provision your network keys in `boards/user_keys.overlay` (as explained in [periodical_uplink sample](../../lbm/periodical_uplink)).

**Build:**
```bash
west build --pristine --board xiao_nrf54l15/nrf54l15/cpuapp --shield semtech_loraplus_expansion_board --shield semtech_wio_lr2021 usp_zephyr/samples/usp/rac/multiprotocol
```

**Flash the firmware:**
```bash
west flash --runner pyocd
```
