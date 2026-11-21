#!/usr/bin/env python3
"""Generate a tiny deterministic, non-fragmented AVC MP4 for demuxer tests."""
import struct
from pathlib import Path


def box(kind, payload):
    return struct.pack(">I4s", len(payload) + 8, kind.encode("ascii")) + payload


def fullbox(kind, body, version=0, flags=0):
    return box(kind, bytes((version, (flags >> 16) & 255, (flags >> 8) & 255, flags & 255)) + body)


def make_movie(chunk_offset):
    avcc = bytes([1, 0x42, 0x00, 0x1E, 0xFF, 0xE1,
                  0x00, 0x04, 0x67, 0x42, 0x00, 0x1E,
                  0x01, 0x00, 0x02, 0x68, 0xCE])
    visual = bytearray(78)
    visual[24:28] = struct.pack(">HH", 320, 240)
    avc1 = box("avc1", bytes(visual) + box("avcC", avcc))
    stsd = fullbox("stsd", struct.pack(">I", 1) + avc1)
    stts = fullbox("stts", struct.pack(">I2I", 1, 2, 1000))
    stsc = fullbox("stsc", struct.pack(">I3I", 1, 1, 2, 1))
    stsz = fullbox("stsz", struct.pack(">II2I", 0, 2, 7, 6))
    stco = fullbox("stco", struct.pack(">II", 1, chunk_offset))
    stss = fullbox("stss", struct.pack(">II", 1, 1))
    stbl = box("stbl", stsd + stts + stsc + stsz + stco + stss)
    minf = box("minf", stbl)
    hdlr = fullbox("hdlr", struct.pack(">I4s12s", 0, b"vide", bytes(12)))
    # version/flags + creation + modification + timescale + duration + language + quality
    mdhd = fullbox("mdhd", struct.pack(">IIIIHH", 0, 0, 1000, 2000, 0, 0))
    mdia = box("mdia", mdhd + hdlr + minf)
    trak = box("trak", mdia)
    return box("moov", trak), stco


# ftyp at the front. stco points to the first byte of the two samples.
ftyp = box("ftyp", b"isom" + struct.pack(">I", 0x200) + b"isom" + b"iso2")
mdat_payload = bytes([0, 0, 0, 3, 0x65, 0xAA, 0xBB,
                      0, 0, 0, 2, 0x41, 0xCC])
moov, _ = make_movie(0)
offset = len(ftyp) + len(moov) + 8
moov, _ = make_movie(offset)
Path("build/test-avc.mp4").write_bytes(ftyp + moov + box("mdat", mdat_payload))
