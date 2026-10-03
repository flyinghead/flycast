#!/usr/bin/env python3
"""Generate a tiny stored 7z file with no file/folder CRC properties."""

from pathlib import Path
import struct
import zlib

payload = b"no checksum"
name = "no-crc.bin".encode("utf-16le") + b"\0\0"
# Header, MainStreamsInfo, PackInfo (one stream), UnpackInfo (one Copy coder),
# SubStreamsInfo, FilesInfo (one UTF-16 name). Every integer fits in one byte.
header = bytes([1, 4, 6, 0, 1, 9, len(payload), 0,
                7, 11, 1, 0, 1, 1, 0, 12, len(payload), 0, 8, 0, 0,
                5, 1, 17, len(name) + 1, 0]) + name + bytes([0, 0])
start = struct.pack("<QQI", len(payload), len(header), zlib.crc32(header))
archive = (b"7z\xbc\xaf\x27\x1c\x00\x04"
           + struct.pack("<I", zlib.crc32(start)) + start + payload + header)
Path(__file__).with_name("no-crc.7z").write_bytes(archive)
