"""A scanline OpenEXR reader, ours, because the one Blender ships lied about a file it wrote itself.

WHY THIS EXISTS. Blender 5.2's compositor file-output node writes OPEN_EXR_MULTILAYER and nothing
else -- the format enum has one member and the per-item override is accepted without effect -- and
`bpy.data.images.load` opens a multilayer file at `size (0, 0)` with `channels 0`. So the loader that
wrote the file cannot read it, and every quantity dumped through it came out as a header with no
samples under it. Measured, both sides, before this file was written.

WHY NOT A PACKAGE. `OpenEXR` and `imageio` are pip dependencies in the one offline script the
constraints allow, for a format that is a header, an offset table and zlib. A dependency is a package
the host provides or it is ours; this is ours, and it is sixty lines.

WHAT IT READS AND WHAT IT REFUSES. Scanline images, `NONE`/`ZIPS`/`ZIP`, `HALF` and `FLOAT` channels.
Tiled, deep and multipart files are REFUSED BY NAME rather than half-read -- this reader serves one
producer and a file it does not understand is a defect in that producer, not an input to accommodate.
"""

import struct
import zlib

import numpy

MAGIC = 0x01312F76
_PIXEL_TYPES = {0: numpy.uint32, 1: numpy.float16, 2: numpy.float32}
_LINES_PER_BLOCK = {0: 1, 2: 1, 3: 16}


class Unreadable(Exception):
    pass


def _attributes(data):
    at = 8
    found = {}
    while True:
        end = data.index(b"\0", at)
        name = data[at:end].decode("ascii")
        at = end + 1
        if not name:
            return found, at
        end = data.index(b"\0", at)
        kind = data[at:end].decode("ascii")
        at = end + 1
        size = struct.unpack_from("<i", data, at)[0]
        at += 4
        found[name] = (kind, data[at:at + size])
        at += size


def _channels(blob):
    at = 0
    out = []
    while at < len(blob) and blob[at] != 0:
        end = blob.index(b"\0", at)
        name = blob[at:end].decode("ascii")
        at = end + 1
        pixel_type = struct.unpack_from("<i", blob, at)[0]
        at += 16
        if pixel_type not in _PIXEL_TYPES:
            raise Unreadable("channel %r has pixel type %d" % (name, pixel_type))
        out.append((name, pixel_type))
    return out


def _unzip(block):
    """zlib, then EXR's own byte predictor and its two-half interleave, in that order."""
    raw = numpy.frombuffer(zlib.decompress(block), dtype=numpy.uint8).astype(numpy.int32)
    raw[1:] -= 128
    numpy.cumsum(raw, out=raw)
    halves = (raw & 0xFF).astype(numpy.uint8)
    out = numpy.empty_like(halves)
    split = (len(halves) + 1) // 2
    out[0::2] = halves[:split]
    out[1::2] = halves[split:]
    return out.tobytes()


def read(path):
    """`(width, height, {channel: float32 array, top row first})`, and the name keeps any layer
    prefix the producer wrote -- stripping it here would merge two layers that differ only by it."""
    with open(path, "rb") as handle:
        data = handle.read()
    if len(data) < 8 or struct.unpack_from("<I", data, 0)[0] != MAGIC:
        raise Unreadable(path + ": not an OpenEXR file")
    flags = struct.unpack_from("<I", data, 4)[0]
    if (flags >> 9) & 1:
        raise Unreadable(path + ": tiled, and this reader serves scanline images")
    if (flags >> 11) & 1 or (flags >> 12) & 1:
        raise Unreadable(path + ": deep or multipart, and this reader serves neither")

    header, at = _attributes(data)
    for wanted in ("channels", "dataWindow", "compression"):
        if wanted not in header:
            raise Unreadable(path + ": the header declares no " + wanted)
    channels = _channels(header["channels"][1])
    x0, y0, x1, y1 = struct.unpack_from("<iiii", header["dataWindow"][1], 0)
    width, height = x1 - x0 + 1, y1 - y0 + 1
    compression = header["compression"][1][0]
    if compression not in _LINES_PER_BLOCK:
        raise Unreadable(path + ": compression %d, and this reader knows NONE, ZIPS and ZIP"
                         % compression)
    per_block = _LINES_PER_BLOCK[compression]

    blocks = (height + per_block - 1) // per_block
    offsets = struct.unpack_from("<%dQ" % blocks, data, at)
    planes = {name: numpy.empty(width * height, dtype=numpy.float32) for name, _ in channels}
    for offset in offsets:
        line = struct.unpack_from("<i", data, offset)[0]
        size = struct.unpack_from("<i", data, offset + 4)[0]
        block = data[offset + 8:offset + 8 + size]
        rows = min(per_block, y1 - line + 1)
        payload = block if compression == 0 else _unzip(block)
        # Within a block the rows come first and the CHANNELS second, in the header's own order --
        # so the offset of a channel's row depends on every channel declared before it.
        cursor = 0
        for row in range(rows):
            for name, pixel_type in channels:
                dtype = _PIXEL_TYPES[pixel_type]
                count = width * numpy.dtype(dtype).itemsize
                values = numpy.frombuffer(payload, dtype=dtype, count=width, offset=cursor)
                planes[name][(line - y0 + row) * width:(line - y0 + row + 1) * width] = values
                cursor += count
    return width, height, {name: planes[name].reshape(height, width) for name, _ in channels}
