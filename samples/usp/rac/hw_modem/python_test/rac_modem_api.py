#!/usr/bin/env python3
"""
Modular RAC Modem API Library
This module provides a clean interface for interacting with the RAC modem
including all API commands: version, open, rac_lora, get_results, close.
"""

import serial
import time
import struct
import sys

# Add serialization directory to path for protobuf imports
sys.path.append('../serialization/generated')

try:
    import smtc_rac_context_pb2
    PROTOBUF_AVAILABLE = True
except ImportError:
    print("Warning: smtc_rac_context_pb2 not found. Run build.sh in serialization directory first.")
    PROTOBUF_AVAILABLE = False

# Command IDs
class RacCommands:
    GET_VERSION = 0x10
    RAC_OPEN = 0xA2
    USP_SUBMIT = 0xA0
    # RAC_GET_RESULTS = 0xA5  # Removed - use NHM protocol instead
    RAC_CLOSE = 0xA3

    # NHM (New Hw Modem) Protocol
    NHM_EXTENDED = 0xA6

# NHM Protocol Constants and Classes
class NHMConstants:
    """NHM (New Hw Modem) Protocol Constants"""

    # Message Types (3 bits - UCI compatible)
    MT_RFU = 0
    MT_COMMAND = 1
    MT_RESPONSE = 2
    MT_NOTIFICATION = 3

    # Packet Boundary Flags (1 bit - UCI compatible)
    PBF_COMPLETE_OR_LAST = 0
    PBF_NOT_LAST = 1

    # Extended Command IDs (12-bit addressing = 4096 commands)
    CMD_USP_SUBMIT = 0x100
    CMD_USP_CAD = 0x101
    CMD_USP_GET_RESULTS = 0x102
    CMD_USP_GET_NEXT_SEGMENT = 0x103

    # Protocol Limits
    HEADER_SIZE = 4
    MAX_PAYLOAD_PER_PACKET = 251  # 255 - 4 header bytes
    MAX_REASSEMBLED_SIZE = 700

class NHMHeader:
    """NHM Protocol Header manipulation"""

    def __init__(self, mt=0, pbf=0, cmd_id=0, length=0):
        self.mt = mt
        self.pbf = pbf
        self.cmd_id = cmd_id
        self.length = length

    def to_bytes(self):
        """Convert header to 4-byte array"""
        # Byte 0: MT[7:5] + PBF[4] + CMD_ID_HIGH[3:0]
        byte0 = ((self.mt & 0x07) << 5) | ((self.pbf & 0x01) << 4) | ((self.cmd_id >> 8) & 0x0F)
        # Byte 1: CMD_ID_LOW[7:0]
        byte1 = self.cmd_id & 0xFF
        # Byte 2: RFU (Reserved)
        byte2 = 0x00
        # Byte 3: Length
        byte3 = self.length & 0xFF

        return bytes([byte0, byte1, byte2, byte3])

    @classmethod
    def from_bytes(cls, data):
        """Parse header from 4-byte array"""
        if len(data) < 4:
            raise ValueError("Header must be at least 4 bytes")

        byte0, byte1, byte2, byte3 = data[:4]

        mt = (byte0 >> 5) & 0x07
        pbf = (byte0 >> 4) & 0x01
        cmd_id = ((byte0 & 0x0F) << 8) | byte1
        length = byte3

        return cls(mt, pbf, cmd_id, length)

class NHMProtocol:
    """NHM Protocol handler for segmentation and message building"""

    def __init__(self, api_instance):
        self.api = api_instance

    def send_nhm_command(self, nhm_cmd_id, payload, expect_response=True):
        """Send NHM command with automatic segmentation if needed

        Args:
            nhm_cmd_id (int): NHM command ID (12-bit)
            payload (bytes): Payload data
            expect_response (bool): Whether to expect a response

        Returns:
            dict: Response from the last segment or complete message
        """
        if len(payload) <= NHMConstants.MAX_PAYLOAD_PER_PACKET:
            # Single packet - no segmentation needed
            return self._send_complete_packet(nhm_cmd_id, payload, expect_response)
        else:
            # Multi-packet - segmentation needed
            return self._send_segmented_packets(nhm_cmd_id, payload, expect_response)

    def _send_complete_packet(self, nhm_cmd_id, payload, expect_response=True):
        """Send complete NHM packet (no segmentation)"""
        # Create NHM header
        header = NHMHeader(
            mt=NHMConstants.MT_COMMAND,
            pbf=NHMConstants.PBF_COMPLETE_OR_LAST,
            cmd_id=nhm_cmd_id,
            length=len(payload)
        )

        # Build NHM packet: Header + Payload
        nhm_packet = header.to_bytes() + payload

        # Send via old protocol CMD_NHM_EXTENDED
        return self.api._send_command(RacCommands.NHM_EXTENDED, nhm_packet)

    def _send_segmented_packets(self, nhm_cmd_id, payload, expect_response=True):
        """Send segmented NHM packets"""
        segments = self._create_segments(payload)
        last_result = None

        for i, segment in enumerate(segments):
            is_last = (i == len(segments) - 1)
            pbf = NHMConstants.PBF_COMPLETE_OR_LAST if is_last else NHMConstants.PBF_NOT_LAST

            # Create NHM header for this segment
            header = NHMHeader(
                mt=NHMConstants.MT_COMMAND,
                pbf=pbf,
                cmd_id=nhm_cmd_id,
                length=len(segment)
            )

            # Build NHM packet: Header + Segment
            nhm_packet = header.to_bytes() + segment

            # Send segment - only expect response from last segment
            expect_resp = expect_response and is_last
            result = self.api._send_command(RacCommands.NHM_EXTENDED, nhm_packet)

            if not result.get("success", False):
                return result  # Error occurred

            last_result = result

            # Small delay between segments
            time.sleep(0.01)

        return last_result

    def _create_segments(self, payload):
        """Split payload into segments"""
        segments = []
        offset = 0

        while offset < len(payload):
            segment_size = min(NHMConstants.MAX_PAYLOAD_PER_PACKET, len(payload) - offset)
            segment = payload[offset:offset + segment_size]
            segments.append(segment)
            offset += segment_size

        return segments

    def receive_nhm_response(self, nhm_cmd_id, max_attempts=1):
        """Receive NHM response with automatic segmentation handling
        
        Args:
            nhm_cmd_id (int): Expected NHM command ID in response
            max_attempts (int): Maximum attempts to get complete response
            
        Returns:
            dict: Complete response data or error
        """
        complete_response = b""
        attempt = 0
        
        while attempt < max_attempts:
            try:
                # Send initial command (already sent by caller)
                if attempt == 0:
                    # First call - just read the response
                    response = self.api.ser.read(300)  # Read up to 300 bytes
                else:
                    # Send GET_NEXT_SEGMENT command
                    result = self.api._send_command(RacCommands.NHM_EXTENDED, 
                        self._create_get_next_segment_packet())
                    if not result.get("success"):
                        return {"error": "Failed to get next segment", "attempt": attempt}
                    response = bytes.fromhex(result.get("raw", ""))
                
                if len(response) < 5:
                    return {"error": "Response too short", "response": response.hex()}
                
                # Parse NHM response header (after bridge header + length)
                # Response format: [bridge_header(3)][length(1)][nhm_header(4)][payload][crc]
                if len(response) < 8:  # 3 bridge + 1 length + 4 nhm header minimum
                    return {"error": "Response too short for NHM header"}
                    
                # Extract NHM header starting at offset 4 (after bridge + length)
                nhm_header = NHMHeader.from_bytes(response[4:8])
                
                # Verify this is the expected response
                if nhm_header.cmd_id != nhm_cmd_id:
                    return {"error": f"Unexpected response CMD_ID: {nhm_header.cmd_id:03x} != {nhm_cmd_id:03x}"}
                
                if nhm_header.mt != NHMConstants.MT_RESPONSE:
                    return {"error": f"Unexpected message type: {nhm_header.mt} (expected {NHMConstants.MT_RESPONSE})"}
                
                # Extract payload from this segment
                payload_start = 8  # 3 bridge + 1 length + 4 nhm header 
                payload_end = payload_start + nhm_header.length
                segment_payload = response[payload_start:payload_end]
                
                # Append to complete response
                complete_response += segment_payload
                
                print(f"🔍 NHM: Received segment {attempt + 1}, {len(segment_payload)} bytes, PBF={nhm_header.pbf}")
                
                # Check if this is the last segment
                if nhm_header.pbf == NHMConstants.PBF_COMPLETE_OR_LAST:
                    # Complete response received
                    print(f"✅ NHM: Complete response received ({len(complete_response)} bytes)")
                    return {
                        "success": True,
                        "payload": complete_response,
                        "segments": attempt + 1,
                        "total_size": len(complete_response)
                    }
                
                # More segments expected
                attempt += 1
                
            except Exception as e:
                return {"error": f"Exception during segment {attempt}: {e}"}
        
        return {"error": f"Incomplete response after {max_attempts} segments"}

    def _create_get_next_segment_packet(self):
        """Create GET_NEXT_SEGMENT NHM packet"""
        header = NHMHeader(
            mt=NHMConstants.MT_COMMAND,
            pbf=NHMConstants.PBF_COMPLETE_OR_LAST,
            cmd_id=NHMConstants.CMD_USP_GET_NEXT_SEGMENT,
            length=0  # No payload
        )
        return header.to_bytes()

class RacModemAPI:
    """Modular API class for RAC modem communication"""

    def __init__(self, port, baudrate=115200, timeout=1):
        """Initialize the modem connection

        Args:
            port (str): Serial port device (e.g., '/dev/ttyUSB0')
            baudrate (int): Communication baud rate
            timeout (float): Serial timeout in seconds
        """
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser = None
        self.radio_handle = None

        # Initialize NHM protocol handler
        self.nhm = NHMProtocol(self)

    def connect(self):
        """Establish serial connection"""
        try:
            self.ser = serial.Serial(port=self.port, baudrate=self.baudrate, timeout=self.timeout)
            return True
        except serial.SerialException as e:
            print(f"❌ Serial connection failed: {e}")
            return False

    def disconnect(self):
        """Close serial connection"""
        if self.ser and self.ser.is_open:
            self.ser.close()

    def _build_packet(self, packet_id: int, length: int, payload: bytes = b'') -> bytes:
        """Build a command packet

        Args:
            packet_id (int): Command ID
            length (int): Payload length
            payload (bytes): Command payload

        Returns:
            bytes: Formatted packet
        """
        if length != len(payload):
            raise ValueError("Length does not match payload size")
        return bytes([packet_id, length]) + payload

    def _send_command(self, packet_id: int, payload: bytes = b'', max_response=100) -> dict:
        """Send a command and return parsed response

        Args:
            packet_id (int): Command ID
            payload (bytes): Command payload
            max_response (int): Maximum response bytes to read

        Returns:
            dict: Parsed response with success/error information
        """
        if not self.ser or not self.ser.is_open:
            return {"error": "Serial connection not established"}

        try:
            # Build and send packet
            packet = self._build_packet(packet_id, len(payload), payload)

            # Debug: Print command sent in hex
            print(f"🔍 TX: {packet.hex()}")

            self.ser.write(packet)
            self.ser.flush()

            # Read and parse response
            response = self.ser.read(max_response)

            # Debug: Print response received in hex
            print(f"🔍 RX: {response.hex()}")

            return self._parse_response(response)

        except Exception as e:
            return {"error": f"Command failed: {e}"}

    def _parse_response(self, response: bytes) -> dict:
        """Parse modem response into structured data

        Args:
            response (bytes): Raw response from modem

        Returns:
            dict: Parsed response data
        """
        if len(response) < 5:  # Minimum: bridge_header(2) + return_code(1) + payload_len(1) + crc(1)
            return {"error": "Response too short", "raw": response.hex()}

        # Extract fields according to observed structure
        bridge_header = struct.unpack('>H', response[0:2])[0]  # Big endian
        return_code = response[2]
        payload_length = response[3]

        if len(response) < 4 + payload_length + 1:
            return {"error": "Response length mismatch", "raw": response.hex()}

        payload = response[4:4+payload_length]
        crc_received = response[4+payload_length]  # CRC on 1 byte

        return {
            "bridge_header": bridge_header,
            "return_code": return_code,
            "payload_length": payload_length,
            "payload": payload,
            "crc_received": crc_received,
            "success": return_code == 0,
            "raw": response.hex()
        }

    # ========================================
    # API COMMAND METHODS
    # ========================================

    def get_version(self) -> dict:
        """Get LBM version

        Returns:
            dict: Response with version information
        """
        result = self._send_command(RacCommands.GET_VERSION)
        if result.get("success") and result.get("payload_length", 0) > 0:
            result["version"] = result["payload"].hex()
        return result

    def rac_open(self, radio_id: int = 0x01) -> dict:
        """Open RAC radio session

        Args:
            radio_id (int): Radio identifier

        Returns:
            dict: Response with radio handle
        """
        result = self._send_command(RacCommands.RAC_OPEN, bytes([radio_id]))

        if result.get("success") and result.get("payload_length", 0) >= 1:
            # Extract radio handle from response
            self.radio_handle = result["payload"][0]
            result["radio_handle"] = self.radio_handle
        else:
            result["radio_handle"] = None

        return result

    def rac_lora(self, context_config: dict = None) -> dict:
        """Execute RAC LoRa transaction

        Args:
            context_config (dict): Configuration for RAC context, or None for default

        Returns:
            dict: Command execution result (no payload expected)
        """
        if not PROTOBUF_AVAILABLE:
            return {"error": "Protobuf not available - cannot create RAC context"}

        try:
            # Create RAC context
            if context_config:
                rac_ctx = self._create_rac_context(context_config)
            else:
                rac_ctx = self._create_default_rac_context()

            # Create LoRa request wrapper
            lora_request = smtc_rac_context_pb2.smtc_rac_lora_request_pb_t()
            lora_request.radio_access_id = self.radio_handle or 0x01  # Use stored handle or default
            lora_request.rac_config.CopyFrom(rac_ctx)  # Copy the context into the request

            # Serialize the request (not just the context)
            serialized_request = lora_request.SerializeToString()

            # Send command
            result = self._send_command(RacCommands.USP_SUBMIT, serialized_request)
            result["context_size"] = len(serialized_request)

            return result

        except Exception as e:
            return {"error": f"Failed to create/send RAC request: {e}"}

    def rac_get_results(self, max_attempts: int = 10, poll_interval: float = 1.0) -> dict:
        """DEPRECATED: Use nhm_rac_get_results() instead

        This method has been removed - use nhm_rac_get_results() which supports
        larger responses and automatic segmentation.
        """
        print("⚠️  DEPRECATED: rac_get_results() removed - using nhm_rac_get_results()")
        return self.nhm_rac_get_results(max_attempts, poll_interval)

    def rac_close(self, radio_handle: int = None) -> dict:
        """Close RAC radio session

        Args:
            radio_handle (int): Radio handle to close, or None to use stored handle

        Returns:
            dict: Command result
        """
        handle = radio_handle or self.radio_handle or 0x01
        result = self._send_command(RacCommands.RAC_CLOSE, bytes([handle]))
        result["radio_handle_used"] = handle
        return result

    def rac_lora_tx(self, payload: bytes, context_config: dict = None) -> dict:
        """Execute RAC LoRa TX transaction

        Args:
            payload (bytes): Payload to transmit
            context_config (dict): Optional configuration, or None for default

        Returns:
            dict: Command execution result
        """
        if not PROTOBUF_AVAILABLE:
            return {"error": "Protobuf not available - cannot create RAC context"}

        try:
            # Create TX context
            if context_config:
                rac_ctx = self._create_rac_context(context_config)
            else:
                rac_ctx = self._create_default_rac_context()

            # Configure for TX
            rac_ctx.radio_params.is_tx = True
            rac_ctx.radio_params.tx_size = len(payload)
            rac_ctx.smtc_rac_data_buffer_setup.tx_payload_buffer = payload

            # Create LoRa request wrapper
            lora_request = smtc_rac_context_pb2.smtc_rac_lora_request_pb_t()
            lora_request.radio_access_id = self.radio_handle or 0x01  # Use stored handle or default
            lora_request.rac_config.CopyFrom(rac_ctx)  # Copy the context into the request

            # Serialize and send the request
            serialized_request = lora_request.SerializeToString()
            result = self._send_command(RacCommands.USP_SUBMIT, serialized_request)
            result["context_size"] = len(serialized_request)
            result["payload_sent"] = payload

            return result

        except Exception as e:
            return {"error": f"Failed to create/send TX request: {e}"}

    def rac_lora_rx(self, rx_timeout_ms: int = 30000, context_config: dict = None) -> dict:
        """Execute RAC LoRa RX transaction

        Args:
            rx_timeout_ms (int): RX timeout in milliseconds
            context_config (dict): Optional configuration, or None for default

        Returns:
            dict: Command execution result
        """
        if not PROTOBUF_AVAILABLE:
            return {"error": "Protobuf not available - cannot create RAC context"}

        try:
            # Create RX context
            if context_config:
                rac_ctx = self._create_rac_context(context_config)
            else:
                rac_ctx = self._create_default_rac_context()

            # Configure for RX
            rac_ctx.radio_params.is_tx = False
            rac_ctx.radio_params.rx_timeout_ms = rx_timeout_ms
            rac_ctx.radio_params.max_rx_size = 255  # Maximum expected RX size

            # Create LoRa request wrapper
            lora_request = smtc_rac_context_pb2.smtc_rac_lora_request_pb_t()
            lora_request.radio_access_id = self.radio_handle or 0x01  # Use stored handle or default
            lora_request.rac_config.CopyFrom(rac_ctx)  # Copy the context into the request

            # Serialize and send the request
            serialized_request = lora_request.SerializeToString()
            result = self._send_command(RacCommands.USP_SUBMIT, serialized_request)
            result["context_size"] = len(serialized_request)
            result["rx_timeout_ms"] = rx_timeout_ms

            return result

        except Exception as e:
            return {"error": f"Failed to create/send RX request: {e}"}

    # ========================================
    # HELPER METHODS
    # ========================================

    def _create_default_rac_context(self):
        """Create a default RAC context

        This matches EXACTLY the configuration from:
        - RF_FREQ_IN_HZ = 868100000 (868.1 MHz)
        - TX_OUTPUT_POWER_DBM = 14
        - LORA_SPREADING_FACTOR = RAL_LORA_SF9
        - LORA_BANDWIDTH = RAL_LORA_BW_500_KHZ
        - LORA_CODING_RATE = RAL_LORA_CR_4_5
        - LORA_PREAMBLE_LENGTH = 12
        - LORA_PKT_LEN_MODE = RAL_LORA_PKT_EXPLICIT
        - LORA_IQ = false
        - LORA_CRC = true
        - LORA_SYNCWORD = LORA_PRIVATE_NETWORK_SYNCWORD
        """
        rac_ctx = smtc_rac_context_pb2.smtc_rac_context_pb_t()

        # Set modulation type to LoRa by default
        rac_ctx.modulation_type = smtc_rac_context_pb2.smtc_rac_modulation_type_pb_t.SMTC_RAC_MODULATION_LORA_PB

        # Radio parameters
        rac_ctx.radio_params.is_tx = True
        rac_ctx.radio_params.is_ranging_exchange = False
        rac_ctx.radio_params.frequency_in_hz = 868100000                          # RF_FREQ_IN_HZ
        rac_ctx.radio_params.tx_power_in_dbm = 14                                 # TX_OUTPUT_POWER_DBM
        rac_ctx.radio_params.sf = smtc_rac_context_pb2.lora_spreading_factor_pb_t.SF9_PB       # LORA_SPREADING_FACTOR = RAL_LORA_SF9
        rac_ctx.radio_params.bw = smtc_rac_context_pb2.lora_bandwidth_pb_t.BW_500_KHZ_PB       # LORA_BANDWIDTH = RAL_LORA_BW_500_KHZ
        rac_ctx.radio_params.cr = smtc_rac_context_pb2.lora_coding_rate_pb_t.CR_4_5_PB         # LORA_CODING_RATE = RAL_LORA_CR_4_5
        rac_ctx.radio_params.preamble_len_in_symb = 12                            # LORA_PREAMBLE_LENGTH = 12
        rac_ctx.radio_params.header_type = smtc_rac_context_pb2.lora_packet_length_mode_pb_t.EXPLICIT_HEADER_PB  # LORA_PKT_LEN_MODE = RAL_LORA_PKT_EXPLICIT
        rac_ctx.radio_params.invert_iq_is_on = False                              # LORA_IQ = false
        rac_ctx.radio_params.crc_is_on = True                                     # LORA_CRC = true
        rac_ctx.radio_params.sync_word = smtc_rac_context_pb2.lora_syncword_pb_t.LORA_PRIVATE_NETWORK_SYNCWORD_PB  # LORA_SYNCWORD = LORA_PRIVATE_NETWORK_SYNCWORD
        rac_ctx.radio_params.rx_timeout_ms = 30000
        rac_ctx.radio_params.max_rx_size = 0  # Default for TX
        rac_ctx.radio_params.tx_size = 19  # Size of "Hello RAC API Test!"

        # Default TX data (input fields only)
        rac_ctx.smtc_rac_data_buffer_setup.tx_payload_buffer = b"Hello RAC API Test!"

        # Default scheduler - use ASAP
        rac_ctx.scheduler_config.scheduling = smtc_rac_context_pb2.smtc_rac_scheduling_pb_t.SMTC_RAC_ASAP_TRANSACTION_PB
        rac_ctx.scheduler_config.start_time_ms = 1000  # 1 second

        return rac_ctx

    def _create_rac_context(self, config: dict):
        """Create RAC context from configuration dictionary

        Args:
            config (dict): Configuration parameters with full LoRa support

        Returns:
            smtc_rac_context_pb_t: Configured context
        """
        rac_ctx = smtc_rac_context_pb2.smtc_rac_context_pb_t()

        # Set modulation type to LoRa by default
        rac_ctx.modulation_type = smtc_rac_context_pb2.smtc_rac_modulation_type_pb_t.SMTC_RAC_MODULATION_LORA_PB

        # Radio parameters - comprehensive support
        radio = config.get("radio", {})
        rac_ctx.radio_params.is_tx = radio.get("is_tx", True)
        rac_ctx.radio_params.is_ranging_exchange = radio.get("is_ranging_exchange", False)
        rac_ctx.radio_params.frequency_in_hz = radio.get("frequency_hz", 868100000)
        rac_ctx.radio_params.tx_power_in_dbm = radio.get("tx_power_dbm", 14)

        # LoRa modulation parameters with full enum support
        if "sf" in radio:
            sf_mapping = {
                "SF5": smtc_rac_context_pb2.lora_spreading_factor_pb_t.SF5_PB,
                "SF6": smtc_rac_context_pb2.lora_spreading_factor_pb_t.SF6_PB,
                "SF7": smtc_rac_context_pb2.lora_spreading_factor_pb_t.SF7_PB,
                "SF8": smtc_rac_context_pb2.lora_spreading_factor_pb_t.SF8_PB,
                "SF9": smtc_rac_context_pb2.lora_spreading_factor_pb_t.SF9_PB,
                "SF10": smtc_rac_context_pb2.lora_spreading_factor_pb_t.SF10_PB,
                "SF11": smtc_rac_context_pb2.lora_spreading_factor_pb_t.SF11_PB,
                "SF12": smtc_rac_context_pb2.lora_spreading_factor_pb_t.SF12_PB
            }
            rac_ctx.radio_params.sf = sf_mapping.get(radio["sf"], smtc_rac_context_pb2.lora_spreading_factor_pb_t.SF9_PB)
        else:
            rac_ctx.radio_params.sf = smtc_rac_context_pb2.lora_spreading_factor_pb_t.SF9_PB  # Default

        if "bw" in radio:
            bw_mapping = {
                "BW_7_8_KHZ": smtc_rac_context_pb2.lora_bandwidth_pb_t.BW_7_8_KHZ_PB,
                "BW_10_4_KHZ": smtc_rac_context_pb2.lora_bandwidth_pb_t.BW_10_4_KHZ_PB,
                "BW_15_6_KHZ": smtc_rac_context_pb2.lora_bandwidth_pb_t.BW_15_6_KHZ_PB,
                "BW_20_8_KHZ": smtc_rac_context_pb2.lora_bandwidth_pb_t.BW_20_8_KHZ_PB,
                "BW_31_25_KHZ": smtc_rac_context_pb2.lora_bandwidth_pb_t.BW_31_25_KHZ_PB,
                "BW_41_7_KHZ": smtc_rac_context_pb2.lora_bandwidth_pb_t.BW_41_7_KHZ_PB,
                "BW_62_5_KHZ": smtc_rac_context_pb2.lora_bandwidth_pb_t.BW_62_5_KHZ_PB,
                "BW_125_KHZ": smtc_rac_context_pb2.lora_bandwidth_pb_t.BW_125_KHZ_PB,
                "BW_250_KHZ": smtc_rac_context_pb2.lora_bandwidth_pb_t.BW_250_KHZ_PB,
                "BW_500_KHZ": smtc_rac_context_pb2.lora_bandwidth_pb_t.BW_500_KHZ_PB
            }
            rac_ctx.radio_params.bw = bw_mapping.get(radio["bw"], smtc_rac_context_pb2.lora_bandwidth_pb_t.BW_500_KHZ_PB)
        else:
            rac_ctx.radio_params.bw = smtc_rac_context_pb2.lora_bandwidth_pb_t.BW_500_KHZ_PB  # Default

        if "cr" in radio:
            cr_mapping = {
                "CR_4_5": smtc_rac_context_pb2.lora_coding_rate_pb_t.CR_4_5_PB,
                "CR_4_6": smtc_rac_context_pb2.lora_coding_rate_pb_t.CR_4_6_PB,
                "CR_4_7": smtc_rac_context_pb2.lora_coding_rate_pb_t.CR_4_7_PB,
                "CR_4_8": smtc_rac_context_pb2.lora_coding_rate_pb_t.CR_4_8_PB
            }
            rac_ctx.radio_params.cr = cr_mapping.get(radio["cr"], smtc_rac_context_pb2.lora_coding_rate_pb_t.CR_4_5_PB)
        else:
            rac_ctx.radio_params.cr = smtc_rac_context_pb2.lora_coding_rate_pb_t.CR_4_5_PB  # Default

        rac_ctx.radio_params.preamble_len_in_symb = radio.get("preamble_length", 12)

        if "header_type" in radio:
            header_mapping = {
                "EXPLICIT": smtc_rac_context_pb2.lora_packet_length_mode_pb_t.EXPLICIT_HEADER_PB,
                "IMPLICIT": smtc_rac_context_pb2.lora_packet_length_mode_pb_t.IMPLICIT_HEADER_PB
            }
            rac_ctx.radio_params.header_type = header_mapping.get(radio["header_type"], smtc_rac_context_pb2.lora_packet_length_mode_pb_t.EXPLICIT_HEADER_PB)
        else:
            rac_ctx.radio_params.header_type = smtc_rac_context_pb2.lora_packet_length_mode_pb_t.EXPLICIT_HEADER_PB

        rac_ctx.radio_params.invert_iq_is_on = radio.get("invert_iq", False)
        rac_ctx.radio_params.crc_is_on = radio.get("crc_enabled", True)

        if "sync_word" in radio:
            sync_mapping = {
                "PRIVATE": smtc_rac_context_pb2.lora_syncword_pb_t.LORA_PRIVATE_NETWORK_SYNCWORD_PB,
                "PUBLIC": smtc_rac_context_pb2.lora_syncword_pb_t.LORA_PUBLIC_NETWORK_SYNCWORD_PB
            }
            rac_ctx.radio_params.sync_word = sync_mapping.get(radio["sync_word"], smtc_rac_context_pb2.lora_syncword_pb_t.LORA_PRIVATE_NETWORK_SYNCWORD_PB)
        else:
            rac_ctx.radio_params.sync_word = smtc_rac_context_pb2.lora_syncword_pb_t.LORA_PRIVATE_NETWORK_SYNCWORD_PB

        rac_ctx.radio_params.rx_timeout_ms = radio.get("rx_timeout_ms", 30000)
        rac_ctx.radio_params.max_rx_size = radio.get("max_rx_size", 255)
        rac_ctx.radio_params.tx_size = radio.get("tx_size", 0)

        # Data buffer setup
        data = config.get("data", {})
        payload = data.get("payload", b"Hello RAC!")
        if isinstance(payload, str):
            payload = payload.encode('utf-8')
        
        # Configure data buffer setup
        if rac_ctx.radio_params.is_tx:
            rac_ctx.smtc_rac_data_buffer_setup.tx_payload_buffer = payload

        # Scheduler
        scheduler = config.get("scheduler", {})
        rac_ctx.scheduler_config.start_time_ms = scheduler.get("start_time_ms", 1000)
        rac_ctx.scheduler_config.scheduling = smtc_rac_context_pb2.smtc_rac_scheduling_pb_t.SMTC_RAC_ASAP_TRANSACTION_PB

        return rac_ctx

    def print_result(self, result: dict, command_name: str = ""):
        """Pretty print command result

        Args:
            result (dict): Command result to print
            command_name (str): Name of the command for display
        """
        if command_name:
            print(f"=== {command_name} ===")

        if "error" in result:
            print(f"❌ Error: {result['error']}")
            if "raw" in result:
                print(f"   Raw response: {result['raw']}")
            return

        print(f"Return code: 0x{result.get('return_code', 0):02x} ({'OK' if result.get('success') else 'ERROR'})")
        print(f"CRC received: 0x{result.get('crc_received', 0):02x}")
        print(f"Success: {result.get('success', False)}")

        # Command-specific information (show contextual results FIRST)
        if "version" in result:
            print(f"LBM Version: 0x{result['version']}")
        elif "radio_handle" in result:
            print(f"Radio handle: 0x{result['radio_handle']:02x}")
        elif "context_size" in result:
            print(f"Context size: {result['context_size']} bytes")
            if "payload_sent" in result:
                print(f"Payload sent: {result['payload_sent']}")
            if "rx_timeout_ms" in result:
                print(f"RX timeout: {result['rx_timeout_ms']} ms")
        elif "parsed_results" in result:
            parsed = result["parsed_results"]
            print(f"Transaction status: {parsed['transaction_status']}")
            print(f"Attempts: {parsed['attempt']}")

            # Display contextual results based on rp_status (operation type)
            rp_status = parsed.get('rp_status', 0)
            rp_status_names = {
                0: "RX_CRC_ERROR", 1: "CAD_POSITIVE", 2: "CAD_NEGATIVE", 3: "TX_DONE",
                4: "RX_PACKET", 5: "RX_TIMEOUT", 6: "LBT_FREE_CHANNEL", 7: "LBT_BUSY_CHANNEL",
                8: "WIFI_SCAN_DONE", 9: "GNSS_SCAN_DONE", 10: "TASK_ABORTED", 11: "TASK_INIT",
                12: "LR_FHSS_HOP", 13: "RTTOF_REQ_DISCARDED", 14: "RTTOF_RESP_DONE",
                15: "RTTOF_EXCH_VALID", 16: "RTTOF_TIMEOUT"
            }
            operation_name = rp_status_names.get(rp_status, f"UNKNOWN_{rp_status}")
            print(f"Operation completed: {operation_name}")

            if parsed.get("transaction_status") == 1:  # RAC_TRANSACTION_COMPLETED_PB
                # Display results based on operation type
                if rp_status == 3:  # TX_DONE
                    print("✅ TX Transmission completed successfully")
                    start_ts = parsed.get('radio_start_timestamp_ms')
                    end_ts = parsed.get('radio_end_timestamp_ms')
                    if start_ts is not None and end_ts is not None:
                        print(f"  Duration: {end_ts - start_ts} ms")

                elif rp_status == 4:  # RX_PACKET
                    print("📡 RX Reception completed - Packet received")
                    if 'rssi_result' in parsed:
                        print(f"  RSSI: {parsed.get('rssi_result', 'N/A')} dBm")
                    if 'snr_result' in parsed:
                        print(f"  SNR: {parsed.get('snr_result', 'N/A')} dB")
                    payload_size = parsed.get('payload_size', 0)
                    if payload_size > 0:
                        print(f"  Payload size: {payload_size} bytes")
                        try:
                            payload_data = parsed.get('payload_data', b'')
                            payload_str = payload_data.decode('utf-8', errors='ignore')
                            print(f"  Payload: '{payload_str}'")
                        except:
                            payload_data = parsed.get('payload_data', b'')
                            print(f"  Payload: {payload_data.hex()}")

                elif rp_status == 5:  # RX_TIMEOUT
                    print("⏰ RX Reception timeout - No packet received")

                elif rp_status in [15, 14]:  # RTTOF_EXCH_VALID, RTTOF_RESP_DONE
                    print("📏 RTToF Ranging completed successfully")
                    if 'ranging_result' in parsed:
                        ranging = parsed['ranging_result']
                        print(f"  Distance: {ranging.get('distance_m', 'N/A')} meters")
                        print(f"  RSSI: {ranging.get('rssi', 'N/A')} dBm")
                        print(f"  Valid: {ranging.get('valid', False)}")
                    else:
                        print("  Ranging data not available")

                elif rp_status == 16:  # RTTOF_TIMEOUT
                    print("⏰ RTToF Ranging timeout")

                elif rp_status in [1, 2]:  # CAD_POSITIVE, CAD_NEGATIVE
                    cad_result = "Channel Activity Detected" if rp_status == 1 else "No Channel Activity"
                    print(f"📻 CAD completed: {cad_result}")

                else:
                    # Generic results display for other operations
                    print(f"Radio operation completed:")
                    if 'rssi_result' in parsed:
                        print(f"  RSSI: {parsed.get('rssi_result', 'N/A')} dBm")
                    if 'snr_result' in parsed:
                        print(f"  SNR: {parsed.get('snr_result', 'N/A')} dB")

                # Show timestamps only if available
                start_ts = parsed.get('radio_start_timestamp_ms')
                end_ts = parsed.get('radio_end_timestamp_ms')
                if start_ts is not None or end_ts is not None:
                    print(f"  Start timestamp: {start_ts or 0} ms")
                    print(f"  End timestamp: {end_ts or 0} ms")
        elif "radio_handle_used" in result:
            print(f"Radio handle used: 0x{result['radio_handle_used']:02x}")

        # Show raw response for debugging (at the end)
        if "raw" in result and result['raw']:
            print(f"Raw response: 0x{result['raw']}")

    # ========================================
    # NHM (New Hw Modem) Protocol Methods
    # ========================================

    def nhm_rac_lora(self, context_config: dict = None) -> dict:
        """Execute RAC LoRa transaction via NHM protocol with automatic segmentation

        This method uses the NHM (New Hw Modem) protocol which supports automatic
        segmentation for payloads larger than 251 bytes.

        Args:
            context_config (dict): Optional configuration, or None for default

        Returns:
            dict: Command execution result
        """
        if not PROTOBUF_AVAILABLE:
            return {"error": "Protobuf not available - cannot create RAC context"}

        try:
            # Create context
            if context_config:
                rac_ctx = self._create_rac_context(context_config)
            else:
                rac_ctx = self._create_default_rac_context()

            # Create LoRa request wrapper
            lora_request = smtc_rac_context_pb2.smtc_rac_lora_request_pb_t()
            lora_request.radio_access_id = self.radio_handle or 0x01
            lora_request.rac_config.CopyFrom(rac_ctx)

            # Serialize payload
            serialized_request = lora_request.SerializeToString()

            print(f"🔍 NHM: Sending RAC LoRa command via NHM protocol ({len(serialized_request)} bytes)")
            if len(serialized_request) > NHMConstants.MAX_PAYLOAD_PER_PACKET:
                print(f"🔀 NHM: Payload requires segmentation ({len(serialized_request)} bytes > {NHMConstants.MAX_PAYLOAD_PER_PACKET} bytes)")

            # Send via NHM protocol (with automatic segmentation if needed)
            result = self.nhm.send_nhm_command(NHMConstants.CMD_USP_SUBMIT, serialized_request)
            result["context_size"] = len(serialized_request)
            result["payload_sent"] = rac_ctx.smtc_rac_data.tx_payload
            result["protocol"] = "NHM"

            return result

        except Exception as e:
            return {"error": f"Failed to create/send NHM LoRa request: {e}"}

    def _parse_rac_results(self, rac_results_pb) -> dict:
        """Parse RAC results protobuf to structured data

        Args:
            rac_results_pb: Parsed protobuf results object

        Returns:
            dict: Structured results data
        """
        # Convert to structured data (extracted from legacy rac_get_results)
        parsed_results = {
            "transaction_status": rac_results_pb.transaction_status,
            "return_code": rac_results_pb.return_code,
            "rp_status": getattr(rac_results_pb, 'rp_status', 11),  # Default to TASK_INIT if missing
        }

        # Add detailed results only if 'results' field is available
        if hasattr(rac_results_pb, 'results') and rac_results_pb.HasField('results'):
            try:
                parsed_results.update({
                    "rssi_result": rac_results_pb.results.rssi_result,
                    "snr_result": rac_results_pb.results.snr_result,
                    "radio_start_timestamp_ms": rac_results_pb.results.radio_start_timestamp_ms,
                    "radio_end_timestamp_ms": rac_results_pb.results.radio_end_timestamp_ms,
                    
                    # TX payload info
                    "tx_payload": rac_results_pb.results.tx_payload,
                    "tx_size": len(rac_results_pb.results.tx_payload),
                    
                    # RX payload info
                    "rx_payload": rac_results_pb.results.rx_payload,
                    "rx_size": len(rac_results_pb.results.rx_payload),
                    "max_rx_size": rac_results_pb.results.max_rx_size,
                    
                    # Legacy fields for backward compatibility
                    "payload_data": (rac_results_pb.results.rx_payload if len(rac_results_pb.results.rx_payload) > 0 
                                   else rac_results_pb.results.tx_payload),
                    "payload_size": max(len(rac_results_pb.results.tx_payload), len(rac_results_pb.results.rx_payload)),
                    "payload": (rac_results_pb.results.rx_payload if len(rac_results_pb.results.rx_payload) > 0 
                              else rac_results_pb.results.tx_payload),
                })

                # Extract ranging results if available
                if hasattr(rac_results_pb.results, 'ranging_result') and rac_results_pb.results.HasField('ranging_result'):
                    ranging_data = rac_results_pb.results.ranging_result
                    parsed_results["ranging_result"] = {
                        "valid": getattr(ranging_data, 'valid', False),
                        "distance_m": getattr(ranging_data, 'distance_m', 0.0),
                        "rssi": getattr(ranging_data, 'rssi', 0.0),
                        "timestamp": getattr(ranging_data, 'timestamp', 0)  # Note: timestamp not timestamp_ms in protobuf
                    }
            except AttributeError as e:
                # Some result fields might not be available
                parsed_results["result_parse_warning"] = f"Some result fields unavailable: {e}"
        else:
            # No 'results' field available - transaction may be pending or failed
            # Set default values for missing fields
            parsed_results.update({
                "rssi_result": 0,
                "snr_result": 0,
                "payload_data": b"",
                "payload_size": 0,
                "radio_start_timestamp_ms": 0,
                "radio_end_timestamp_ms": 0
            })

        return parsed_results

    def nhm_rac_get_results(self, max_attempts: int = 10, poll_interval: float = 1.0) -> dict:
        """Poll for RAC transaction results via NHM protocol with segmentation support

        This method uses the NHM protocol to get results, which can handle
        responses larger than 255 bytes through automatic segmentation.

        Args:
            max_attempts (int): Maximum polling attempts
            poll_interval (float): Polling interval in seconds

        Returns:
            dict: Results with transaction status and radio data
        """
        if not PROTOBUF_AVAILABLE:
            return {"error": "Protobuf not available - cannot parse results"}

        print(f"🔍 NHM: Polling for results via NHM protocol (max {max_attempts} attempts)")

        for attempt in range(max_attempts):
            try:
                # Send GET_RESULTS command via NHM protocol  
                # Build NHM command manually to send
                header = NHMHeader(
                    mt=NHMConstants.MT_COMMAND,
                    pbf=NHMConstants.PBF_COMPLETE_OR_LAST,
                    cmd_id=NHMConstants.CMD_USP_GET_RESULTS,
                    length=0  # No payload
                )
                nhm_packet = header.to_bytes()
                
                # Send via old protocol wrapper but read response manually
                if not self.ser or not self.ser.is_open:
                    return {"error": "Serial connection not established"}

                try:
                    packet = self._build_packet(RacCommands.NHM_EXTENDED, len(nhm_packet), nhm_packet)
                    print(f"🔍 TX: {packet.hex()}")
                    self.ser.write(packet)
                    self.ser.flush()

                    # Read response directly - don't use _parse_response()
                    response_bytes = self.ser.read(300)  # Read up to 300 bytes
                    print(f"🔍 RX: {response_bytes.hex()}")
                    
                    if len(response_bytes) < 8:  # 3 bridge + 1 length + 4 nhm header minimum
                        print(f"  Attempt {attempt + 1}/{max_attempts}: Response too short for NHM")
                        if attempt < max_attempts - 1:
                            time.sleep(poll_interval)
                            continue
                        else:
                            return {"error": "Response too short", "raw": response_bytes.hex()}

                except Exception as e:
                    print(f"  Attempt {attempt + 1}/{max_attempts}: Command send failed - {e}")
                    if attempt < max_attempts - 1:
                        time.sleep(poll_interval)
                        continue
                    else:
                        return {"error": f"Command send failed: {e}"}

                # Now handle potential segmentation in the response
                # Parse the NHM response header (starting after bridge header + length)
                try:
                    payload_length = response_bytes[3]  # Length byte at offset 3
                    nhm_header = NHMHeader.from_bytes(response_bytes[4:8])  # NHM header at offset 4-7
                    
                    # Verify this is a GET_RESULTS response
                    if nhm_header.cmd_id != NHMConstants.CMD_USP_GET_RESULTS:
                        print(f"  Attempt {attempt + 1}/{max_attempts}: Unexpected CMD_ID: {nhm_header.cmd_id:03x}")
                        if attempt < max_attempts - 1:
                            time.sleep(poll_interval)
                            continue
                        else:
                            return {"error": f"Unexpected CMD_ID: {nhm_header.cmd_id:03x}"}
                    
                    if nhm_header.mt != NHMConstants.MT_RESPONSE:
                        print(f"  Attempt {attempt + 1}/{max_attempts}: Not a response (MT={nhm_header.mt})")
                        if attempt < max_attempts - 1:
                            time.sleep(poll_interval)
                            continue
                        else:
                            return {"error": f"Not a response: MT={nhm_header.mt}"}

                    # Handle segmented response
                    complete_payload = b""
                    
                    # Extract first segment payload  
                    payload_start = 8  # 3 bridge + 1 length + 4 nhm header
                    first_segment = response_bytes[payload_start:payload_start + nhm_header.length]
                    complete_payload += first_segment
                    
                    print(f"🔍 NHM: First segment received ({len(first_segment)} bytes), PBF={nhm_header.pbf}")

                    # Check if response is segmented
                    if nhm_header.pbf == NHMConstants.PBF_NOT_LAST:
                        print("🔍 NHM: Response is segmented, requesting additional segments...")
                        
                        # Get remaining segments
                        segment_num = 2
                        while True:
                            # Send GET_NEXT_SEGMENT command manually
                            next_header_cmd = NHMHeader(
                                mt=NHMConstants.MT_COMMAND,
                                pbf=NHMConstants.PBF_COMPLETE_OR_LAST,
                                cmd_id=NHMConstants.CMD_USP_GET_NEXT_SEGMENT,
                                length=0  # No payload
                            )
                            next_packet = self._build_packet(RacCommands.NHM_EXTENDED, 4, next_header_cmd.to_bytes())
                            
                            print(f"🔍 TX: {next_packet.hex()}")
                            self.ser.write(next_packet)
                            self.ser.flush()

                            next_response = self.ser.read(300)
                            print(f"🔍 RX: {next_response.hex()}")
                            
                            if len(next_response) < 8:
                                return {"error": f"Segment {segment_num} too short", "protocol": "NHM"}
                            
                            # Parse next segment header (format: [bridge(3)][length(1)][nhm_header(4)][payload][crc])
                            next_nhm_header = NHMHeader.from_bytes(next_response[4:8])
                            next_payload = next_response[8:8 + next_nhm_header.length]
                            complete_payload += next_payload
                            
                            print(f"🔍 NHM: Segment {segment_num} received ({len(next_payload)} bytes), PBF={next_nhm_header.pbf}")
                            
                            # Check if this is the last segment
                            if next_nhm_header.pbf == NHMConstants.PBF_COMPLETE_OR_LAST:
                                print(f"✅ NHM: Complete segmented response received ({len(complete_payload)} bytes total)")
                                break
                            
                            segment_num += 1
                            if segment_num > 10:  # Safety limit
                                return {"error": "Too many segments (>10)", "protocol": "NHM"}
                    
                    # Parse the complete protobuf payload
                    try:
                        rac_results = smtc_rac_context_pb2.rac_results_pb_t()
                        rac_results.ParseFromString(complete_payload)
                        parsed_results = self._parse_rac_results(rac_results)
                        parsed_results["attempt"] = attempt + 1
                        
                        # Build result dictionary
                        result = {
                            "success": True,
                            "parsed_results": parsed_results,
                            "protocol": "NHM",
                            "response_size": len(complete_payload),
                            "raw": response_bytes.hex()
                        }
                        
                        # Check if results are ready
                        if rac_results.transaction_status == smtc_rac_context_pb2.rac_transaction_status_pb_t.RAC_TRANSACTION_COMPLETED_PB:
                            print(f"✅ NHM: Results ready after {attempt + 1} attempt(s)")
                            return result
                        else:
                            print(f"  Attempt {attempt + 1}/{max_attempts}: Transaction pending")
                    
                    except Exception as e:
                        # Protobuf parsing failed - show debug info
                        hex_str = complete_payload.hex()
                        try:
                            ascii_str = ''.join(chr(b) if 32 <= b <= 126 else '.' for b in complete_payload)
                        except:
                            ascii_str = '(binary data)'

                        print(f"❌ Protobuf parsing failed!")
                        print(f"🔍 Complete payload HEX: {hex_str}")
                        print(f"🔍 Complete payload ASCII: '{ascii_str}'")
                        print(f"🔍 Payload length: {len(complete_payload)} bytes")
                        print(f"🔍 Error: {e}")

                        return {"error": f"Error parsing protobuf: {e}", "success": False, "protocol": "NHM"}

                except Exception as e:
                    print(f"  Attempt {attempt + 1}/{max_attempts}: NHM header parse error - {e}")

                # Wait before next attempt (if not the last one)
                if attempt < max_attempts - 1:
                    time.sleep(poll_interval)

            except Exception as e:
                print(f"  Attempt {attempt + 1}/{max_attempts}: Exception - {e}")
                if attempt == max_attempts - 1:
                    return {"error": f"Polling failed after {max_attempts} attempts: {e}"}
                time.sleep(poll_interval)

        return {"error": f"No results after {max_attempts} polling attempts"}

    def test_nhm_segmentation(self, payload_size: int = 400) -> dict:
        """Test NHM segmentation with a large payload

        Args:
            payload_size (int): Size of test payload in bytes

        Returns:
            dict: Test result
        """
        if not PROTOBUF_AVAILABLE:
            return {"error": "Protobuf not available"}

        # Create test payload
        test_payload = bytes(range(payload_size % 256)) * (payload_size // 256 + 1)
        test_payload = test_payload[:payload_size]  # Trim to exact size

        context_config = {
            "data": {"payload": test_payload}
        }

        print(f"🧪 Testing NHM segmentation with {payload_size}-byte payload")

        # Send via NHM
        result = self.nhm_rac_lora(context_config)
        result["test_payload_size"] = payload_size
        result["test_type"] = "NHM_segmentation"

        return result
