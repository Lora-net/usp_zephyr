#!/usr/bin/env python3
"""
Ping-Pong protocol test using RacModemAPI with NHM support.

This Python implementation follows the exact logic of the C ping_pong application
(app_ping_pong.c) to ensure compatibility and consistent behavior.

Protocol Logic (matching C application):
- Manager: Sends PING, waits for PONG with strict validation
- Subordinate: Waits for PING, sends PONG

Validation Rules (identical to C):
- Manager: Expects PONG + separator=0 + correct counter
- Subordinate: Expects PING + separator=0 (any counter accepted)
- Error handling: consecutive_fails counting + retry/restart logic
- Messages: Same format and content as C application

Features:
- Strict payload validation matching C application behavior
- Automatic mode detection (manager by default)
- Configurable timing parameters synchronized with subordinate
- Error handling and retry logic identical to C
- NHM protocol support for large payloads

Setup Instructions:
====================

For a complete ping-pong test, you need TWO devices:

1. PYTHON MANAGER (this script):
   python test_ping_pong.py /dev/ttyUSB0 --mode manager

2. C SUBORDINATE (compiled firmware):
   Compile the C subordinate application with synchronized timing (5s processing delay):
   
   west build --pristine --board nucleo_l476rg/stm32l476xx --shield semtech_lr1120mb1xxs usp_zephyr/samples/lora_mp/sdk/ping_pong/ -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_C_FLAGS="-DSUBORDINATE_PROCESSING_TIME_MS=5000"
   
   Then flash to the subordinate device:
   west flash

Usage:
    python test_ping_pong.py /dev/ttyUSB0 --mode manager    # Start as manager (sends first PING)
    python test_ping_pong.py /dev/ttyUSB0 --mode subordinate # Start as subordinate (waits for PING)

Note: The timing parameters in this script are synchronized with SUBORDINATE_PROCESSING_TIME_MS=5000
"""

import argparse
import time
import struct
from rac_modem_api import RacModemAPI, PROTOBUF_AVAILABLE

# Ping Pong Protocol Constants (matching C implementation)
PREFIX_SIZE = 4
SEPARATOR_SIZE = 1
COUNTER_SIZE = 1
PAYLOAD_SIZE = PREFIX_SIZE + SEPARATOR_SIZE + COUNTER_SIZE

PREFIX_OFFSET = 0
SEPARATOR_OFFSET = PREFIX_OFFSET + PREFIX_SIZE
COUNTER_OFFSET = SEPARATOR_OFFSET + SEPARATOR_SIZE

PING_MESSAGE = b"PING"
PONG_MESSAGE = b"PONG"

# Timing constants (ms) - SYNCHRONIZED with subordinate 5s delay
# These parameters match the C subordinate compiled with -DSUBORDINATE_PROCESSING_TIME_MS=5000
# Compile command: west build --pristine --board nucleo_l476rg/stm32l476xx --shield semtech_lr1120mb1xxs usp_zephyr/samples/lora_mp/sdk/ping_pong/ -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_C_FLAGS="-DSUBORDINATE_PROCESSING_TIME_MS=5000"
DELAY_BETWEEN_TX = 3000         # Delay between TX transactions
PROCESSING_TIME = 100          # Processing time for subordinate response
MANAGER_RX_TIMEOUT = 18000     # RX timeout for manager (5s subordinate delay + margin)
SUBORDINATE_RX_TIMEOUT = 30000 # RX timeout for subordinate
INTER_CYCLE_DELAY = 1000       # Reduced delay between cycles

# Protocol limits
MAX_EXCHANGE_COUNT = 25        # Maximum ping-pong exchanges
MAX_RETRY_COUNT = 5           # Maximum consecutive failures

# Radio configuration (matching ping_pong example and apps_configuration.h)
PING_PONG_CONFIG = {
    "radio": {
        "is_tx": True,
        "frequency_hz": 868100000,  # 868.1 MHz
        "tx_power_dbm": 14,
        "sf": "SF9",
        "bw": "BW_125_KHZ",         # ← CORRIGÉ: ping_pong natif utilise 125 kHz !
        "cr": "CR_4_5",
        "preamble_length": 12,
        "header_type": "EXPLICIT",
        "invert_iq": False,
        "crc_enabled": True,
        "sync_word": "PRIVATE",
        "rx_timeout_ms": 30000,     # Subordinate timeout, sera ajusté dynamiquement
        "max_rx_size": 6            # Ping-pong payload size (4 chars + 1 separator + 1 counter)
    },
    "scheduler": {
        "start_time_ms": 1000  # Will be updated for each transaction
    }
}

class PingPongState:
    """Ping Pong state machine"""

    def __init__(self, is_manager=True):
        self.is_manager = is_manager
        self.counter = 0
        self.consecutive_fails = 0
        self.tx_requested = is_manager  # Manager starts with TX, subordinate starts with RX

    def reset(self):
        """Reset to subordinate state"""
        print("🔄 Resetting to subordinate mode...")
        self.is_manager = False
        self.counter = 0
        self.consecutive_fails = 0
        self.tx_requested = False

    def should_restart(self):
        """Check if we should restart the protocol"""
        return (self.counter >= MAX_EXCHANGE_COUNT or
                self.consecutive_fails >= MAX_RETRY_COUNT)

class PingPongProtocol:
    """Implements the ping-pong protocol using RacModemAPI"""

    def __init__(self, api: RacModemAPI, is_manager=True):
        self.api = api
        self.state = PingPongState(is_manager)

    def parse_ping_pong_payload(self, payload: bytes) -> dict:
        """Parse ping-pong payload: PREFIX + SEPARATOR + COUNTER

        Args:
            payload (bytes): Received payload

        Returns:
            dict: Parsed payload info or error
        """
        if len(payload) < PAYLOAD_SIZE:
            return {"error": f"Payload too short: {len(payload)} < {PAYLOAD_SIZE}"}

        # Extract components
        prefix = payload[PREFIX_OFFSET:PREFIX_OFFSET + PREFIX_SIZE]
        separator = payload[SEPARATOR_OFFSET:SEPARATOR_OFFSET + SEPARATOR_SIZE]
        counter_bytes = payload[COUNTER_OFFSET:COUNTER_OFFSET + COUNTER_SIZE]

        # Strict validation like C application
        # 1. Validate separator (must be 0)
        if separator != b'\x00':
            return {"error": f"Invalid separator: expected 0x00, got {separator.hex()}", "valid": False}

        # 2. Extract counter
        counter = counter_bytes[0] if counter_bytes else 0

        # 3. Validate message prefix 
        if prefix == PING_MESSAGE:
            message_type = "PING"
        elif prefix == PONG_MESSAGE:
            message_type = "PONG"
        else:
            return {"error": f"Unknown prefix: expected PING/PONG, got {prefix}", "valid": False}

        return {
            "type": message_type,
            "counter": counter,
            "valid": True
        }

    def build_ping_pong_payload(self, message_type: str, counter: int) -> bytes:
        """Build ping-pong payload

        Args:
            message_type (str): "PING" or "PONG"
            counter (int): Counter value

        Returns:
            bytes: Formatted payload
        """
        if message_type == "PING":
            prefix = PING_MESSAGE
        elif message_type == "PONG":
            prefix = PONG_MESSAGE
        else:
            raise ValueError(f"Invalid message type: {message_type}")

        payload = bytearray(PAYLOAD_SIZE)
        payload[PREFIX_OFFSET:PREFIX_OFFSET + PREFIX_SIZE] = prefix
        payload[SEPARATOR_OFFSET] = 0  # Separator
        payload[COUNTER_OFFSET] = counter & 0xFF

        return bytes(payload)

    def send_ping_pong_tx(self, delay_ms: int = 0) -> dict:
        """Send ping-pong TX transaction and wait for completion

        Args:
            delay_ms (int): Delay before transmission

        Returns:
            dict: Transaction result with TX completion status
        """
        # Determine message type
        message_type = "PING" if self.state.is_manager else "PONG"
        # Build payload
        payload = self.build_ping_pong_payload(message_type, self.state.counter)

        # Update timing
        config = PING_PONG_CONFIG.copy()
        config["scheduler"]["start_time_ms"] = delay_ms if delay_ms > 0 else 10

        print(f"📤 Sending {message_type} (counter={self.state.counter}, delay={delay_ms}ms)")

        # Step 1: Send TX command
        result = self.api.rac_lora_tx(payload, config)

        if not result.get("success"):
            print(f"   ❌ Failed to send {message_type}: {result.get('error', 'Unknown error')}")
            self.state.consecutive_fails += 1
            return result

        print(f"   ✅ {message_type} command accepted")

        # Step 2: Wait for TX to complete with get_results
        print(f"   🔍 Waiting for TX completion...")
        tx_results = self.api.nhm_rac_get_results(max_attempts=30, poll_interval=0.1)

        if not tx_results.get("success"):
            print(f"   ❌ Failed to get TX results: {tx_results.get('error', 'Unknown')}")
            self.state.consecutive_fails += 1
            return tx_results

        # Check TX completion status
        parsed = tx_results.get("parsed_results", {})
        tx_status = parsed.get("transaction_status", 0)

        if tx_status == 1:  # TX_DONE
            print(f"   ✅ TX DONE - {message_type} transmission completed successfully")
            return {"success": True, "tx_results": tx_results}
        else:
            print(f"   ❌ TX FAILED - {message_type} transmission failed (status: {tx_status})")
            self.state.consecutive_fails += 1
            return {"success": False, "error": f"TX failed with status {tx_status}", "tx_results": tx_results}

    def wait_for_rx(self, timeout_ms: int) -> dict:
        """Wait for RX transaction and poll results

        Args:
            timeout_ms (int): RX timeout

        Returns:
            dict: RX result with parsed payload
        """
        # Update timing and ensure RX timeout is used
        config = PING_PONG_CONFIG.copy()
        config["radio"]["rx_timeout_ms"] = timeout_ms  # Override config timeout with parameter
        config["scheduler"]["start_time_ms"] = 0  # RX should start immediately (no delay)

        print(f"📥 Starting RX (timeout={timeout_ms}ms)")

        # Send RX command
        result = self.api.rac_lora_rx(rx_timeout_ms=timeout_ms, context_config=config)

        if not result.get("success"):
            print(f"   ❌ Failed to start RX: {result.get('error', 'Unknown error')}")
            return result

        # Poll for results
        print("   🔍 Polling for RX results...")
        results = self.api.nhm_rac_get_results(max_attempts=60, poll_interval=0.5)  # Up to 30s polling

        if not results.get("success"):
            print(f"   ❌ Failed to get RX results: {results.get('error', 'Unknown error')}")
            self.state.consecutive_fails += 1
            return results

        # Check if we have results
        parsed = results.get("parsed_results", {})
        if parsed.get("transaction_status") != 1:  # Not COMPLETED
            print("   ⏰ RX TIMEOUT - No data received")
            self.state.consecutive_fails += 1
            return {"error": "RX timeout", "timeout": True}

        # Check radio status (rp_status)
        rp_status = parsed.get('rp_status', 0)

        if rp_status == 4:  # RX_PACKET - Successfully received packet
            payload_data = parsed.get("payload_data", b"")
            payload_size = parsed.get('payload_size', 0)
            
            # Always show received data for debugging
            print(f"   🔍 DEBUG: RX packet received - payload_size={payload_size}")
            if payload_size > 0 and payload_data:
                # Display payload in hex and ASCII
                hex_str = payload_data.hex()
                try:
                    ascii_str = ''.join(chr(b) if 32 <= b <= 126 else '.' for b in payload_data)
                except:
                    ascii_str = '(binary data)'
                print(f"   🔍 DEBUG: Payload HEX: {hex_str}")
                print(f"   🔍 DEBUG: Payload ASCII: '{ascii_str}'")
                
                payload_info = self.parse_ping_pong_payload(payload_data)
                results["parsed_payload"] = payload_info

                if payload_info.get("valid"):
                    print(f"   ✅ RX DONE - Received {payload_info['type']} (counter={payload_info['counter']})")
                    # Display RSSI/SNR if available
                    rssi = parsed.get('rssi_result', 'N/A')
                    snr = parsed.get('snr_result', 'N/A')
                    print(f"   📊 RSSI: {rssi} dBm, SNR: {snr} dB")
                else:
                    print(f"   ❌ RX FAILED - Invalid payload: {payload_info.get('error', 'Unknown')}")
                    print(f"   🔍 DEBUG: Full parsed results: {parsed}")
                    self.state.consecutive_fails += 1
            else:
                print("   ❌ RX FAILED - No payload data received")
                print(f"   🔍 DEBUG: Full parsed results: {parsed}")
                self.state.consecutive_fails += 1
                results["parsed_payload"] = {"error": "No payload data", "valid": False}

        elif rp_status == 5:  # RX_TIMEOUT
            print("   ⏰ RX TIMEOUT - No packet received within timeout")
            print(f"   🔍 DEBUG: Full parsed results: {parsed}")
            self.state.consecutive_fails += 1
            results["parsed_payload"] = {"error": "RX timeout", "valid": False}
            
        else:
            print(f"   ❌ RX FAILED - Unexpected radio status: {rp_status}")
            print(f"   🔍 DEBUG: Full parsed results: {parsed}")
            self.state.consecutive_fails += 1
            results["parsed_payload"] = {"error": f"Unexpected status {rp_status}", "valid": False}

        return results

    def handle_received_message(self, payload_info: dict):
        """Handle received ping-pong message with strict validation like C application

        Args:
            payload_info (dict): Parsed payload information
        """
        if not payload_info.get("valid"):
            print("   ❌ ERROR: unexpected payload (invalid format)")
            self.state.consecutive_fails += 1
            return

        received_counter = payload_info["counter"]
        message_type = payload_info["type"]
        payload_is_correct = True

        # Strict payload validation like C application
        if self.state.is_manager:
            # Manager expects PONG with correct counter
            payload_is_correct &= (message_type == "PONG")
            payload_is_correct &= (received_counter == self.state.counter)
            
            if not payload_is_correct:
                print(f"   ❌ ERROR: unexpected payload (expected PONG counter={self.state.counter}, got {message_type} counter={received_counter})")
                self.state.consecutive_fails += 1
                return
            
            print(f"   ✅ payload is correct, continuing (PONG counter={received_counter})")
            self.state.counter += 1
            self.state.consecutive_fails = 0

        else:
            # Subordinate expects PING (any counter is accepted)
            payload_is_correct &= (message_type == "PING")
            
            if not payload_is_correct:
                print(f"   ❌ ERROR: unexpected payload (expected PING, got {message_type})")
                self.state.consecutive_fails += 1
                return
                
            print(f"   ✅ payload is correct, continuing (PING counter={received_counter})")
            # Subordinate uses received counter (as per C application)
            self.state.counter = received_counter
            self.state.consecutive_fails = 0

    def run_manager_cycle(self):
        """Run one manager cycle: TX PING -> RX PONG"""
        print(f"\n👑 Manager cycle {self.state.counter + 1}")

        # Send PING
        result = self.send_ping_pong_tx(DELAY_BETWEEN_TX)

        if not result.get("success"):
            print(f"❌ TX failed, aborting cycle. Error: {result.get('error', 'Unknown')}")
            return

        time.sleep(0.5)  # Brief delay before starting RX

        # Wait for PONG
        result = self.wait_for_rx(MANAGER_RX_TIMEOUT)
        if result.get("timeout"):
            return

        # Handle received message
        payload_info = result.get("parsed_payload", {})
        self.handle_received_message(payload_info)

        # Delay before next cycle to prevent scheduler conflicts
        print(f"⏰ Waiting {INTER_CYCLE_DELAY}ms before next cycle...")
        time.sleep(INTER_CYCLE_DELAY / 1000.0)

    def run_subordinate_cycle(self):
        """Run one subordinate cycle: RX PING -> TX PONG"""
        print(f"\n🤖 Subordinate cycle")

        # Wait for PING
        result = self.wait_for_rx(SUBORDINATE_RX_TIMEOUT)
        if result.get("timeout"):
            return

        # Handle received message
        payload_info = result.get("parsed_payload", {})
        self.handle_received_message(payload_info)

        if payload_info.get("valid") and payload_info.get("type") == "PING":
            time.sleep(PROCESSING_TIME / 1000.0)  # Processing time

            # Send PONG
            self.send_ping_pong_tx(PROCESSING_TIME)

            # Delay after subordinate TX
            print(f"⏰ Waiting {INTER_CYCLE_DELAY}ms after TX...")
            time.sleep(INTER_CYCLE_DELAY / 1000.0)

    def run(self):
        """Run the ping-pong protocol"""
        print(f"🏁 Starting Ping Pong Protocol")
        print(f"   Mode: {'👑 Manager' if self.state.is_manager else '🤖 Subordinate'}")
        print(f"   Max exchanges: {MAX_EXCHANGE_COUNT}")
        print(f"   Max retries: {MAX_RETRY_COUNT}")
        print("   Press Ctrl+C to stop\n")

        try:
            while True:
                # Check restart conditions
                if self.state.should_restart():
                    if self.state.counter >= MAX_EXCHANGE_COUNT:
                        print(f"📊 max exchange count reached, restarting")
                    if self.state.consecutive_fails >= MAX_RETRY_COUNT:
                        print(f"❌ max retry count reached, restarting")

                    print("🔄 resetting parameters and restarting loop")
                    print("🤖 this device is now subordinate")
                    self.state.reset()
                    time.sleep(1)
                    continue

                # Show retry info (like C application)
                if self.state.consecutive_fails > 0:
                    print(f"⚠️  retrying (fails={self.state.consecutive_fails}, max={MAX_RETRY_COUNT})")

                # Run appropriate cycle
                if self.state.is_manager:
                    self.run_manager_cycle()
                else:
                    self.run_subordinate_cycle()

                time.sleep(0.1)  # Brief pause between cycles

        except KeyboardInterrupt:
            print(f"\n🛑 Ping Pong stopped by user")
        except Exception as e:
            print(f"\n❌ Ping Pong error: {e}")

def main():
    """Main function"""
    parser = argparse.ArgumentParser(description="RAC Ping Pong Test")
    parser.add_argument("port", help="Serial port device (e.g., /dev/ttyUSB0)")
    parser.add_argument("--mode", choices=['manager', 'subordinate'], default='manager',
                       help="Ping pong mode (default: manager)")
    parser.add_argument("--baudrate", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--timeout", type=float, default=1.0, help="Serial timeout (default: 1.0s)")

    args = parser.parse_args()

    if not PROTOBUF_AVAILABLE:
        print("❌ Protobuf not available - run ./build.sh in serialization directory")
        return 1

    print(f"🔌 Connecting to {args.port} at {args.baudrate} baud...")
    
    # Show compilation instructions for C subordinate
    if args.mode == 'manager':
        print("\n📋 SETUP REMINDER:")
        print("   For complete ping-pong test, compile C subordinate with:")
        print("   west build --pristine --board nucleo_l476rg/stm32l476xx --shield semtech_lr1120mb1xxs \\")
        print("     usp_zephyr/samples/lora_mp/sdk/ping_pong/ -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \\")
        print("     -DCMAKE_C_FLAGS=\"-DSUBORDINATE_PROCESSING_TIME_MS=5000\"")
        print("   Then: west flash\n")

    # Initialize API
    api = RacModemAPI(args.port, args.baudrate, args.timeout)

    try:
        # Connect and setup
        if not api.connect():
            print("❌ Failed to establish serial connection")
            return 1

        print("✅ Serial connection established")

        # Initialize modem
        print("📋 Getting modem version...")
        result = api.get_version()
        if not result.get("success"):
            print("❌ Failed to get modem version")
            return 1

        print("🔓 Opening RAC session...")
        result = api.rac_open()
        if not result.get("success"):
            print("❌ Failed to open RAC session")
            return 1

        # Start ping pong
        is_manager = (args.mode == 'manager')
        protocol = PingPongProtocol(api, is_manager=is_manager)
        protocol.run()

        # Cleanup
        print("🔒 Closing RAC session...")
        api.rac_close()

        return 0

    except Exception as e:
        print(f"❌ Unexpected error: {e}")
        return 1
    finally:
        api.disconnect()
        print("🔌 Serial connection closed")

if __name__ == "__main__":
    exit(main())
