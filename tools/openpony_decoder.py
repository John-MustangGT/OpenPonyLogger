#!/usr/bin/env python3
"""
OpenPony Binary Log Decoder

Provides classes and utilities for decoding .opl (OpenPony Log) binary files.
Supports session headers, compressed data blocks, and sensor sample parsing.

Usage:
    from openpony_decoder import OpenPonyDecoder
    
    decoder = OpenPonyDecoder('session_12345.opl')
    session = decoder.read_session_header()
    
    for block in decoder.read_blocks():
        for sample in block.samples:
            print(sample.timestamp_utc, sample.type, sample.data)
"""

import struct
import zlib
import uuid
from dataclasses import dataclass
from typing import List, Optional, Tuple, BinaryIO
from datetime import datetime, timezone


class OpenPonyError(Exception):
    """Base exception for OpenPony decoder errors"""
    pass


class InvalidMagicError(OpenPonyError):
    """Raised when file magic number is incorrect"""
    pass


class CRCMismatchError(OpenPonyError):
    """Raised when CRC32 checksum validation fails"""
    pass


class UnsupportedVersionError(OpenPonyError):
    """Raised when file format version is not supported"""
    pass


@dataclass
class SessionHeader:
    """Session start header (68 bytes)"""
    magic: int                    # 0x53545230 ("STR0")
    version: int                  # Format version (0x01)
    startup_id: bytes             # UUID (16 bytes)
    esp_time_at_start: int        # ESP timer at startup (microseconds)
    gps_utc_at_lock: int          # GPS UTC time at lock (seconds since epoch, 0=unknown)
    mac_addr: bytes               # MAC address (6 bytes)
    fw_sha: bytes                 # Firmware git SHA (8 bytes)
    startup_counter: int          # Monotonic startup counter
    crc32: int                    # CRC32 of header
    
    @property
    def startup_uuid(self) -> str:
        """Get startup ID as UUID string"""
        return str(uuid.UUID(bytes=self.startup_id))
    
    @property
    def mac_address(self) -> str:
        """Get MAC address as formatted string"""
        return ':'.join(f'{b:02x}' for b in self.mac_addr)
    
    @property
    def firmware_sha(self) -> str:
        """Get firmware SHA as hex string"""
        return self.fw_sha.hex()
    
    @property
    def has_gps_time(self) -> bool:
        """Check if GPS time correlation is available"""
        return self.gps_utc_at_lock > 0
    
    def esp_to_utc(self, esp_time_us: int) -> Optional[datetime]:
        """Convert ESP timer timestamp to UTC datetime"""
        if not self.has_gps_time:
            return None
        
        esp_delta_us = esp_time_us - self.esp_time_at_start
        utc_seconds = self.gps_utc_at_lock + (esp_delta_us / 1_000_000)
        return datetime.fromtimestamp(utc_seconds, tz=timezone.utc)


@dataclass
class BlockHeader:
    """Data block header (48 bytes)"""
    magic: int                    # 0x4C4F4742 ("LOGB")
    version: int                  # Block version (0x01)
    startup_id: bytes             # UUID (16 bytes)
    timestamp_us: int             # ESP timer at block creation (microseconds)
    uncompressed_size: int        # Size before compression
    compressed_size: int          # Size of compressed payload
    crc32: int                    # CRC32 of compressed data


@dataclass
class Sample:
    """Parsed sensor sample"""
    type: str                     # 'accel', 'gyro', 'compass', 'gps', etc.
    timestamp_us: int             # ESP timer timestamp (microseconds)
    data: dict                    # Type-specific data fields
    
    def to_utc(self, session: SessionHeader) -> Optional[datetime]:
        """Convert sample timestamp to UTC using session correlation"""
        return session.esp_to_utc(self.timestamp_us)


@dataclass
class DataBlock:
    """Decoded data block with samples"""
    header: BlockHeader
    samples: List[Sample]


class OpenPonyDecoder:
    """
    Decoder for OpenPony binary log files (.opl)
    
    Example:
        decoder = OpenPonyDecoder('logfile.opl')
        session = decoder.read_session_header()
        
        for block in decoder.read_blocks():
            for sample in block.samples:
                utc = sample.to_utc(session)
                print(f"{utc}: {sample.type} = {sample.data}")
    """
    
    SESSION_MAGIC = 0x53545230  # "STR0"
    BLOCK_MAGIC = 0x4C4F4742    # "LOGB"
    SUPPORTED_VERSION = 0x01
    
    # Sample type IDs
    SAMPLE_ACCEL = 0x01
    SAMPLE_GYRO = 0x02
    SAMPLE_COMPASS = 0x03
    SAMPLE_GPS = 0x04
    SAMPLE_GPS_SATS = 0x05
    SAMPLE_OBD = 0x06
    SAMPLE_BATTERY = 0x07
    
    def __init__(self, filepath: str):
        """
        Initialize decoder with .opl file path
        
        Args:
            filepath: Path to .opl binary log file
        """
        self.filepath = filepath
        self._file: Optional[BinaryIO] = None
        self._session: Optional[SessionHeader] = None
    
    def __enter__(self):
        """Context manager entry"""
        self._file = open(self.filepath, 'rb')
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit"""
        if self._file:
            self._file.close()
            self._file = None
    
    def read_session_header(self) -> SessionHeader:
        """
        Read and validate session header from file
        
        Returns:
            SessionHeader object with session metadata
            
        Raises:
            InvalidMagicError: If magic number is incorrect
            UnsupportedVersionError: If version is not supported
            CRCMismatchError: If CRC32 validation fails
        """
        if not self._file:
            raise OpenPonyError("File not opened - use context manager")
        
        # Read header (68 bytes)
        self._file.seek(0)
        header_data = self._file.read(68)
        
        if len(header_data) < 68:
            raise OpenPonyError(f"File too small for session header: {len(header_data)} bytes")
        
        # Unpack header
        magic, version = struct.unpack('<IB', header_data[0:5])
        
        if magic != self.SESSION_MAGIC:
            raise InvalidMagicError(f"Invalid session magic: 0x{magic:08X}")
        
        if version != self.SUPPORTED_VERSION:
            raise UnsupportedVersionError(f"Unsupported version: 0x{version:02X}")
        
        startup_id = header_data[8:24]
        esp_time_at_start, = struct.unpack('<q', header_data[24:32])
        gps_utc_at_lock, = struct.unpack('<q', header_data[32:40])
        mac_addr = header_data[40:46]
        fw_sha = header_data[46:54]
        startup_counter, = struct.unpack('<I', header_data[54:58])
        crc_stored, = struct.unpack('<I', header_data[64:68])
        
        # Verify CRC32
        crc_calc = zlib.crc32(header_data[0:64]) & 0xFFFFFFFF
        if crc_stored != crc_calc:
            raise CRCMismatchError(f"Session header CRC mismatch: {crc_stored:08X} != {crc_calc:08X}")
        
        self._session = SessionHeader(
            magic=magic,
            version=version,
            startup_id=startup_id,
            esp_time_at_start=esp_time_at_start,
            gps_utc_at_lock=gps_utc_at_lock,
            mac_addr=mac_addr,
            fw_sha=fw_sha,
            startup_counter=startup_counter,
            crc32=crc_stored
        )
        
        return self._session
    
    def read_blocks(self) -> List[DataBlock]:
        """
        Read all data blocks from file
        
        Returns:
            List of DataBlock objects containing samples
            
        Raises:
            CRCMismatchError: If block CRC validation fails
            OpenPonyError: If decompression fails
        """
        if not self._file:
            raise OpenPonyError("File not opened - use context manager")
        
        if not self._session:
            raise OpenPonyError("Must call read_session_header() first")
        
        blocks = []
        
        while True:
            block_header_data = self._file.read(48)
            
            if len(block_header_data) < 48:
                break  # End of file
            
            # Parse block header
            header = self._parse_block_header(block_header_data)
            
            if header.magic != self.BLOCK_MAGIC:
                break  # End of valid blocks
            
            # Verify UUID matches session
            if header.startup_id != self._session.startup_id:
                raise OpenPonyError("Block UUID mismatch with session")
            
            # Read compressed payload
            compressed_data = self._file.read(header.compressed_size)
            
            if len(compressed_data) != header.compressed_size:
                raise OpenPonyError(f"Incomplete block: expected {header.compressed_size}, got {len(compressed_data)}")
            
            # Verify CRC32
            crc_calc = zlib.crc32(compressed_data) & 0xFFFFFFFF
            if header.crc32 != crc_calc:
                raise CRCMismatchError(f"Block CRC mismatch: {header.crc32:08X} != {crc_calc:08X}")
            
            # Decompress
            try:
                uncompressed_data = zlib.decompress(compressed_data)
            except zlib.error as e:
                raise OpenPonyError(f"Decompression failed: {e}")
            
            if len(uncompressed_data) != header.uncompressed_size:
                raise OpenPonyError(f"Decompressed size mismatch: expected {header.uncompressed_size}, got {len(uncompressed_data)}")
            
            # Parse samples
            samples = self._parse_samples(uncompressed_data, header.timestamp_us)
            
            blocks.append(DataBlock(header=header, samples=samples))
        
        return blocks
    
    def _parse_block_header(self, data: bytes) -> BlockHeader:
        """Parse block header from 48 bytes"""
        magic, = struct.unpack('<I', data[0:4])
        version = data[4]
        startup_id = data[8:24]
        timestamp_us, = struct.unpack('<q', data[24:32])
        uncompressed_size, = struct.unpack('<I', data[32:36])
        compressed_size, = struct.unpack('<I', data[36:40])
        crc32, = struct.unpack('<I', data[40:44])
        
        return BlockHeader(
            magic=magic,
            version=version,
            startup_id=startup_id,
            timestamp_us=timestamp_us,
            uncompressed_size=uncompressed_size,
            compressed_size=compressed_size,
            crc32=crc32
        )
    
    def _parse_samples(self, data: bytes, block_timestamp_us: int) -> List[Sample]:
        """Parse sensor samples from decompressed data"""
        samples = []
        offset = 0
        
        while offset < len(data):
            if offset + 5 > len(data):  # Need at least type + timestamp
                break
            
            sample_type = data[offset]
            offset += 1
            
            timestamp_delta, = struct.unpack('<I', data[offset:offset+4])
            offset += 4
            
            sample_timestamp_us = block_timestamp_us + timestamp_delta
            
            # Parse based on sample type
            if sample_type == self.SAMPLE_ACCEL:
                if offset + 12 > len(data):
                    break
                x, y, z = struct.unpack('<fff', data[offset:offset+12])
                offset += 12
                samples.append(Sample(
                    type='accel',
                    timestamp_us=sample_timestamp_us,
                    data={'x': x, 'y': y, 'z': z, 'unit': 'm/s²'}
                ))
            
            elif sample_type == self.SAMPLE_GYRO:
                if offset + 12 > len(data):
                    break
                x, y, z = struct.unpack('<fff', data[offset:offset+12])
                offset += 12
                samples.append(Sample(
                    type='gyro',
                    timestamp_us=sample_timestamp_us,
                    data={'x': x, 'y': y, 'z': z, 'unit': 'rad/s'}
                ))
            
            elif sample_type == self.SAMPLE_COMPASS:
                if offset + 12 > len(data):
                    break
                x, y, z = struct.unpack('<fff', data[offset:offset+12])
                offset += 12
                samples.append(Sample(
                    type='compass',
                    timestamp_us=sample_timestamp_us,
                    data={'x': x, 'y': y, 'z': z, 'unit': 'μT'}
                ))
            
            elif sample_type == self.SAMPLE_GPS:
                if offset + 32 > len(data):
                    break
                lat, lon = struct.unpack('<dd', data[offset:offset+16])
                offset += 16
                alt, speed, heading, hdop = struct.unpack('<ffff', data[offset:offset+16])
                offset += 16
                samples.append(Sample(
                    type='gps',
                    timestamp_us=sample_timestamp_us,
                    data={
                        'latitude': lat,
                        'longitude': lon,
                        'altitude': alt,
                        'speed': speed,
                        'heading': heading,
                        'hdop': hdop
                    }
                ))
            
            else:
                # Unknown sample type - skip to avoid parsing errors
                # This could be extended to handle other sample types
                break
        
        return samples
    
    @property
    def session(self) -> Optional[SessionHeader]:
        """Get session header if already read"""
        return self._session


# Convenience function
def decode_file(filepath: str) -> Tuple[SessionHeader, List[DataBlock]]:
    """
    Decode entire .opl file in one call
    
    Args:
        filepath: Path to .opl file
        
    Returns:
        Tuple of (SessionHeader, List[DataBlock])
        
    Example:
        session, blocks = decode_file('logfile.opl')
        print(f"Session UUID: {session.startup_uuid}")
        print(f"Total blocks: {len(blocks)}")
    """
    with OpenPonyDecoder(filepath) as decoder:
        session = decoder.read_session_header()
        blocks = decoder.read_blocks()
        return session, blocks


if __name__ == '__main__':
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python openpony_decoder.py <file.opl>")
        sys.exit(1)
    
    filepath = sys.argv[1]
    
    print(f"Decoding: {filepath}")
    print()
    
    try:
        session, blocks = decode_file(filepath)
        
        print("Session Information:")
        print(f"  UUID: {session.startup_uuid}")
        print(f"  MAC Address: {session.mac_address}")
        print(f"  Firmware SHA: {session.firmware_sha}")
        print(f"  Startup Counter: {session.startup_counter}")
        print(f"  ESP Start Time: {session.esp_time_at_start} μs")
        
        if session.has_gps_time:
            gps_time = datetime.fromtimestamp(session.gps_utc_at_lock, tz=timezone.utc)
            print(f"  GPS Lock Time: {gps_time.isoformat()}")
        else:
            print(f"  GPS Lock Time: Not available")
        
        print()
        print(f"Data Blocks: {len(blocks)}")
        
        total_samples = sum(len(block.samples) for block in blocks)
        print(f"Total Samples: {total_samples}")
        
        # Count by type
        sample_counts = {}
        for block in blocks:
            for sample in block.samples:
                sample_counts[sample.type] = sample_counts.get(sample.type, 0) + 1
        
        print()
        print("Sample Breakdown:")
        for sample_type, count in sorted(sample_counts.items()):
            print(f"  {sample_type}: {count}")
        
        # Show first few samples
        print()
        print("First 10 samples:")
        count = 0
        for block in blocks:
            for sample in block.samples:
                utc = sample.to_utc(session)
                time_str = utc.isoformat() if utc else f"{sample.timestamp_us} μs"
                print(f"  {time_str} - {sample.type}: {sample.data}")
                count += 1
                if count >= 10:
                    break
            if count >= 10:
                break
        
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
