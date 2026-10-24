"""Read one existing native ground page without fetching, generating or mutating assets."""

import argparse
import ctypes
import ctypes.util
import hashlib
import json
import math
import sqlite3
import struct
import zlib
from contextlib import closing
from pathlib import Path

import numpy as np

BYTES_MOST = 64 * 1024**2


def package(cache, key):
    with closing(
        sqlite3.connect(cache.resolve().as_uri() + "?mode=ro", uri=True)
    ) as db:
        row = db.execute(
            "SELECT p.rowid,length(p.bytes),p.native_bytes,p.codec,p.crc,a.offset,a.bytes "
            "FROM assets a JOIN packages p ON a.package=p.key "
            "WHERE a.key=? AND a.kind='ground-region'",
            (key,),
        ).fetchone()
        if row is None:
            raise ValueError("existing native ground-region key required")
        rowid, stored_size, native_size, codec, crc, offset, size = row
        if not 0 < stored_size <= BYTES_MOST or not 0 < native_size <= BYTES_MOST:
            raise ValueError("bounded cached package required before blob read")
        if offset != 0 or size != native_size:
            raise ValueError("single complete ground-region package required")
        with db.blobopen("packages", "bytes", rowid, readonly=True) as blob:
            stored = blob.read()
    if len(stored) != stored_size or zlib.crc32(stored) != crc:
        raise ValueError("cached package size/CRC mismatch")
    if codec == 0:
        data = stored
    elif codec == 1:
        library = ctypes.util.find_library("zstd")
        if not library:
            raise ValueError("existing system libzstd required")
        lib = ctypes.CDLL(library)
        lib.ZSTD_decompress.argtypes = [
            ctypes.c_void_p,
            ctypes.c_size_t,
            ctypes.c_void_p,
            ctypes.c_size_t,
        ]
        lib.ZSTD_decompress.restype = ctypes.c_size_t
        lib.ZSTD_isError.argtypes = [ctypes.c_size_t]
        decoded = ctypes.create_string_buffer(native_size)
        used = lib.ZSTD_decompress(decoded, native_size, stored, len(stored))
        if lib.ZSTD_isError(used) or used != native_size:
            raise ValueError("invalid cached zstd package")
        data = decoded.raw
    else:
        raise ValueError("known native package codec required")
    if len(data) != native_size:
        raise ValueError("native package length mismatch")
    return data, {
        "asset_key": key,
        "stored_bytes": stored_size,
        "native_bytes": native_size,
        "stored_sha256": hashlib.sha256(stored).hexdigest(),
        "native_sha256": hashlib.sha256(data).hexdigest(),
    }


class PrefixReader:
    def __init__(self, data):
        self.data, self.at = data, 0

    def read(self, fmt):
        size = struct.calcsize("<" + fmt)
        chunk = self.bytes(size)
        return struct.unpack("<" + fmt, chunk)

    def bytes(self, size):
        if size < 0 or size > len(self.data) - self.at:
            raise ValueError("truncated native ground prefix")
        start, self.at = self.at, self.at + size
        return memoryview(self.data)[start : self.at]

    def array(self, width):
        return self.bytes(self.read("I")[0] * width)


def select_page(data):
    outer = PrefixReader(data)
    version, ground_size = outer.read("QQ")
    if version != 0x35524E474F:
        raise ValueError("native region package version5 required")
    reader = PrefixReader(outer.bytes(ground_size))
    if reader.read("Q")[0] != 0x0005314E5247:
        raise ValueError("native ground-region version5 required")
    anchor = reader.read("dd")
    reader.bytes(8 + 4 + 8 + 3 * 8)
    reader.bytes(8 * 8 + 8 + 4 + 2 * 24 * 4)
    reader.array(4)
    reader.array(1)
    pages = reader.read("I")[0]
    if not 0 < pages <= 65536:
        raise ValueError("bounded nonempty native page sequence required")
    lon, lat = anchor
    anchor_xy = (
        (lon + 180) / 360,
        (1 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2,
    )
    selected, best = None, None
    for index in range(pages):
        zoom, x, y, side, postings, virtual, source_zoom = reader.read("iIIiIBi")
        nodes = reader.array(4)
        if not 0 <= zoom <= 23 or x >= 2**zoom or y >= 2**zoom:
            raise ValueError("valid native tile identity required")
        if side != 33 or postings < 33 or source_zoom < -1 or len(nodes) != 35 * 35 * 4:
            continue
        distance = sum(
            (a - b) ** 2
            for a, b in zip(anchor_xy, ((x + 0.5) / 2**zoom, (y + 0.5) / 2**zoom))
        )
        rank = (-zoom, distance, index)
        if best is None or rank < best:
            best = rank
            selected = (
                {
                    "page": str(index),
                    "zoom": zoom,
                    "x": x,
                    "y": y,
                    "side": side,
                    "postings": postings,
                    "virtual": virtual,
                    "source_zoom": source_zoom,
                    "halo": 1,
                    "nodes": 35 * 35,
                },
                bytes(nodes),
            )
    if selected is None:
        raise ValueError("existing 33-node page required")
    row, heights = selected
    if not np.isfinite(np.frombuffer(heights, dtype="<f4")).all():
        raise ValueError("finite native height words required")
    return (
        row,
        heights,
        {
            "anchor_lon_lat": anchor,
            "pages_in_prefix": pages,
            "selection": "maximum zoom, then nearest tile centre to existing region anchor, then original page index",
            "scope": "Read-only extraction of versioned page prefix; trailing classes/network/geometry are not independently revalidated.",
        },
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--key", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    data, provenance = package(args.cache, args.key)
    row, heights, scope = select_page(data)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / (row["page"] + ".f32")).write_bytes(heights)
    fractions = np.arange(33, dtype=np.float64) / 32
    if not row["virtual"] and row["source_zoom"] < 0:
        postings = row["postings"]
        fractions = (np.arange(33, dtype=np.int64) * (postings - 1) // 32) / (
            postings - 1
        )
    np.savetxt(args.output / (row["page"] + ".fractions"), fractions, fmt="%.17g")
    record = {
        "row": row,
        **provenance,
        **scope,
        "height_sha256": hashlib.sha256(heights).hexdigest(),
    }
    (args.output / "input.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps(record))


if __name__ == "__main__":
    main()
