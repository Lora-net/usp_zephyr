#!/usr/bin/env python3
"""
RAC Modem Test Script - Legacy and NHM Protocol Testing
This script provides comprehensive testing for all RAC modem API commands
using both the legacy protocol and the new NHM (New Hw Modem) protocol.

Features:
- Legacy Protocol: Original hw_modem commands (limited to ~255 bytes)
- NHM Protocol: New segmentation-capable protocol (up to 700 bytes)
- Automatic segmentation for large payloads
- Protocol comparison testing
- Interactive mode for individual command testing

Test Modes:
- workflow: Complete test with small payload (legacy) + maximum payload (NHM with segmentation)
- interactive: Manual command selection and testing

Default Workflow Test Sequence:
1. Get modem version
2. Open RAC session
3. RAC lora transaction (small payload ~33 bytes - legacy protocol)
4. Get results
5. RAC lora transaction (maximum payload 255 bytes - NHM protocol with segmentation)
6. Get results
7. RAC close

Example Usage:
- Complete test: python test_hw_modem.py /dev/ttyUSB0
- Interactive mode: python test_hw_modem.py /dev/ttyUSB0 --mode interactive
"""

import argparse
import time
from rac_modem_api import RacModemAPI, PROTOBUF_AVAILABLE


def test_complete_workflow(api: RacModemAPI):
    """Run complete RAC workflow with both small and maximum payloads

    Sequence:
    1. Get modem version
    2. Open RAC session
    3. RAC lora transaction (small payload - legacy protocol)
    4. Get results
    5. RAC lora transaction (maximum payload - NHM protocol with segmentation)
    6. Get results
    7. RAC close

    Args:
        api (RacModemAPI): API instance
    """
    print("🧪 Running Complete RAC Workflow - Legacy + NHM Protocol Testing")
    print("=" * 70)

    if not PROTOBUF_AVAILABLE:
        print("❌ Cannot run workflow without protobuf support")
        return False

    # Test 1: Get Version
    print("\n📋 Test 1: Get LBM Version")
    result = api.get_version()
    api.print_result(result)
    print("Expected: 0x010000030409001e = 0x0100 (bridge header) + 0x00 (OK) + 0x03 (3 bytes) + 0x040900 (version) + 0x1e (CRC)")

    if not result.get("success"):
        print("❌ Cannot continue without successful version check")
        return False

    # Test 2: RAC Open Session
    print("\n🔓 Test 2: Open RAC Session")
    result = api.rac_open(radio_id=0x01)
    api.print_result(result)

    if not result.get("success"):
        print("❌ Cannot continue without successful radio open")
        return False

    # Test 3: Small Payload Transaction (Legacy Protocol)
    print("\n📡 Test 3: RAC LoRa Transaction - Small Payload (Legacy Protocol)")
    print("-" * 70)

    small_context_config = {
        "radio": {
            "is_tx": True,
            "frequency_hz": 868100000,
            "tx_power_dbm": 14,
            "sf": "SF9",
            "bw": "BW_500_KHZ",
            "cr": "CR_4_5",
            "preamble_length": 12,
            "header_type": "EXPLICIT",
            "invert_iq": False,
            "crc_enabled": True,
            "sync_word": "PRIVATE",
            "rx_timeout_ms": 30000,
            "max_rx_size": 255  # Default buffer size for generic tests
        },
        "data": {
            "payload": "Hello RAC Small Payload Test!"  # ~33 bytes
        },
        "scheduler": {
            "start_time_ms": 1000
        }
    }

    result = api.rac_lora(small_context_config)
    api.print_result(result, "Legacy Protocol - Small Payload")

    if result.get("success"):
        print("\n🔍 Test 4: Get Results - Small Payload")
        result = api.nhm_rac_get_results(max_attempts=15, poll_interval=0.5)
        api.print_result(result, "NHM Get Results (Legacy Payload)")
    else:
        print("❌ Failed to execute small payload LoRa transaction")
        # Continue with large payload test anyway

    # Test 5: Maximum Payload Transaction (NHM Protocol with Segmentation)
    print("\n📡 Test 5: RAC LoRa Transaction - Maximum Payload (NHM Protocol)")
    print("-" * 70)

    # Create maximum payload (255 bytes) to trigger segmentation
    max_payload = "X" * 255

    max_context_config = {
        "radio": {
            "is_tx": True,
            "frequency_hz": 868100000,
            "tx_power_dbm": 14,
            "sf": "SF9",
            "bw": "BW_500_KHZ",
            "cr": "CR_4_5",
            "preamble_length": 12,
            "header_type": "EXPLICIT",
            "invert_iq": False,
            "crc_enabled": True,
            "sync_word": "PRIVATE",
            "rx_timeout_ms": 30000,
            "max_rx_size": 255  # Maximum buffer size for large payload tests
        },
        "data": {
            "payload": max_payload
        },
        "scheduler": {
            "start_time_ms": 2000  # 2 seconds delay
        }
    }

    print(f"🔀 Using NHM protocol for large payload ({len(max_payload)} bytes)")
    result = api.nhm_rac_lora(max_context_config)
    api.print_result(result, "NHM Protocol - Maximum Payload")

    if result.get("success"):
        print("\n🔍 Test 6: Get Results - Maximum Payload")
        result = api.nhm_rac_get_results(max_attempts=15, poll_interval=0.5)
        api.print_result(result, "NHM Get Results")
    else:
        print("❌ Failed to execute maximum payload LoRa transaction")

    # Test 7: RAC Close Session
    print("\n🔒 Test 7: Close RAC Session")
    result = api.rac_close()
    api.print_result(result)

    print("\n✅ Complete workflow finished - Both protocols tested")
    print("📊 Summary:")
    print("   • Legacy LoRa: Small payload (~33 bytes) via CMD_USP_SUBMIT (0xA0)")
    print("   • NHM LoRa: Maximum payload (255 bytes) via CMD_NHM_EXTENDED (0xA6→0x100)")
    print("   • NHM Get Results: All results via CMD_NHM_EXTENDED (0xA6→0x102)")
    print("   • Segmentation: Automatic for payloads > 251 bytes")
    return True

def test_individual_commands(api: RacModemAPI):
    """Test individual commands for debugging/development

    Args:
        api (RacModemAPI): API instance
    """
    print("🔧 Individual Command Testing Mode")
    print("=" * 50)

    while True:
        print("\nAvailable commands:")
        print("1. Get Version (0x10)")
        print("2. RAC Open (0xA2)")
        print("3. RAC LoRa Legacy (0xA0)")
        print("4. NHM Get Results (0xA6→0x102)")
        print("5. RAC Close (0xA3)")
        print("6. Complete Workflow (Small + Max Payload)")
        print("7. NHM RAC LoRa with Custom Payload (0xA6)")
        print("8. NHM Get Results (0xA6)")
        print("0. Exit")

        try:
            choice = input("Enter command number: ").strip()

            if choice == '0':
                break
            elif choice == '1':
                result = api.get_version()
                api.print_result(result, "Get Version")
            elif choice == '2':
                radio_id = input("Radio ID (hex, default 0x01): ").strip() or "0x01"
                radio_id = int(radio_id, 16) if radio_id.startswith('0x') else int(radio_id)
                result = api.rac_open(radio_id)
                api.print_result(result, "RAC Open")
            elif choice == '3':
                if PROTOBUF_AVAILABLE:
                    payload = input("Payload text (default 'Test Message'): ").strip() or "Test Message"
                    context_config = {
                        "data": {"payload": payload}
                    }
                    result = api.rac_lora(context_config)
                    api.print_result(result, "RAC LoRa")
                else:
                    print("❌ Protobuf not available")
            elif choice == '4':
                if PROTOBUF_AVAILABLE:
                    max_attempts = input("Max polling attempts (default 10): ").strip() or "10"
                    max_attempts = int(max_attempts)
                    result = api.nhm_rac_get_results(max_attempts=max_attempts)
                    api.print_result(result, "NHM Get Results")
                else:
                    print("❌ Protobuf not available")
            elif choice == '5':
                result = api.rac_close()
                api.print_result(result, "RAC Close")
            elif choice == '6':
                test_complete_workflow(api)
            elif choice == '7':
                if PROTOBUF_AVAILABLE:
                    print("NHM Protocol Test - Custom Payload")
                    payload = input("Payload text (default 'NHM Test Message'): ").strip() or "NHM Test Message"
                    size_test = input("Test large payload? (y/n): ").strip().lower()

                    if size_test == 'y':
                        size = input("Payload size in bytes (default 300): ").strip() or "300"
                        size = int(size)
                        payload = "X" * size
                        print(f"Creating {size}-byte payload for segmentation test")

                    context_config = {
                        "data": {"payload": payload}
                    }
                    result = api.nhm_rac_lora(context_config)
                    api.print_result(result, "NHM RAC LoRa")
                else:
                    print("❌ Protobuf not available")
            elif choice == '8':
                if PROTOBUF_AVAILABLE:
                    max_attempts = input("Max polling attempts (default 10): ").strip() or "10"
                    max_attempts = int(max_attempts)
                    result = api.nhm_rac_get_results(max_attempts=max_attempts)
                    api.print_result(result, "NHM Get Results")
                else:
                    print("❌ Protobuf not available")
            else:
                print("❌ Invalid choice")

        except KeyboardInterrupt:
            print("\n🛑 Interrupted by user")
            break
        except Exception as e:
            print(f"❌ Error: {e}")


def main():
    """Main function with command line argument parsing"""
    parser = argparse.ArgumentParser(description="RAC Modem Test Script")
    parser.add_argument("port", help="Serial port device (e.g., /dev/ttyUSB0)")
    parser.add_argument("--baudrate", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--timeout", type=float, default=1.0, help="Serial timeout (default: 1.0s)")
    parser.add_argument("--mode", choices=['workflow', 'interactive'],
                        default='workflow', help="Test mode (default: workflow)")

    args = parser.parse_args()

    print(f"🔌 Connecting to {args.port} at {args.baudrate} baud...")

    # Initialize API
    api = RacModemAPI(args.port, args.baudrate, args.timeout)

    try:
        # Connect to modem
        if not api.connect():
            print("❌ Failed to establish serial connection")
            return 1

        print("✅ Serial connection established")

        # Run selected test mode
        if args.mode == 'workflow':
            test_complete_workflow(api)
        elif args.mode == 'interactive':
            test_individual_commands(api)

        return 0

    except KeyboardInterrupt:
        print("\n🛑 Test interrupted by user")
        return 1
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
        return 1
    finally:
        api.disconnect()
        print("🔌 Serial connection closed")

if __name__ == "__main__":
    exit(main())
