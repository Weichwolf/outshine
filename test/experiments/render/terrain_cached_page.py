"""Read existing native ground pages without fetching, generating or mutating assets."""

import argparse
import ctypes
import ctypes.util
import hashlib
import json
import math
import sys
from pathlib import Path

import struct
import zlib
from contextlib import closing

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "transport"))
from native_package_storage import open_packages

BYTES_MOST = 64 * 1024**2


def package(cache, key):
    with closing(
        open_packages(cache.resolve().as_uri() + "?mode=ro", uri=True)
    ) as db:
        row = db.execute(
            "SELECT p.rowid,p.stored_bytes,p.native_bytes,p.codec,p.crc,a.offset,a.bytes,p.body_table "
            "FROM assets a JOIN native_packages p ON a.package=p.key "
            "WHERE a.key=? AND a.kind='ground-region'",
            (key,),
        ).fetchone()
        if row is None:
            raise ValueError("existing native ground-region key required")
        rowid, stored_size, native_size, codec, crc, offset, size, body_table = row
        if not 0 < stored_size <= BYTES_MOST or not 0 < native_size <= BYTES_MOST:
            raise ValueError("bounded cached package required before blob read")
        if offset != 0 or size != native_size:
            raise ValueError("single complete ground-region package required")
        with db.blobopen(body_table, "bytes", rowid, readonly=True) as blob:
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


def page_prefix(data):
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
    return reader, anchor, pages


def page_records(reader, pages):
    for index in range(pages):
        zoom, x, y, side, postings, virtual, source_zoom = reader.read("iIIiIBi")
        nodes = reader.array(4)
        if not 0 <= zoom <= 23 or x >= 2**zoom or y >= 2**zoom:
            raise ValueError("valid native tile identity required")
        yield (
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
                "nodes": len(nodes) // 4,
            },
            nodes,
        )


def eligible(row):
    return (
        row["side"] == 33
        and row["postings"] >= 33
        and row["source_zoom"] >= -1
        and row["nodes"] == 35 * 35
    )


def checked_heights(nodes):
    if not np.isfinite(np.frombuffer(nodes, dtype="<f4")).all():
        raise ValueError("finite native height words required")
    return bytes(nodes)


def scope(anchor, pages, selection):
    return {
        "anchor_lon_lat": anchor,
        "pages_in_prefix": pages,
        "selection": selection,
        "scope": "Read-only extraction of versioned page prefix; trailing classes/network/geometry are not independently revalidated.",
    }


def select_page(data):
    reader, anchor, pages = page_prefix(data)
    lon, lat = anchor
    anchor_xy = (
        (lon + 180) / 360,
        (1 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2,
    )
    selected, best = None, None
    for row, nodes in page_records(reader, pages):
        if not eligible(row):
            continue
        zoom, x, y, index = row["zoom"], row["x"], row["y"], int(row["page"])
        distance = sum(
            (a - b) ** 2
            for a, b in zip(anchor_xy, ((x + 0.5) / 2**zoom, (y + 0.5) / 2**zoom))
        )
        rank = (-zoom, distance, index)
        if best is None or rank < best:
            best = rank
            selected = row, nodes
    if selected is None:
        raise ValueError("existing 33-node page required")
    row, heights = selected
    return (
        row,
        checked_heights(heights),
        scope(
            anchor,
            pages,
            "maximum zoom, then nearest tile centre to existing region anchor, then original page index",
        ),
    )


def select_window(data, zoom, low_x, high_x, low_y, high_y):
    if (
        not 0 <= zoom <= 23
        or not 0 <= low_x <= high_x < 2**zoom
        or not 0 <= low_y <= high_y < 2**zoom
    ):
        raise ValueError("valid explicit native tile window required")
    if (high_x - low_x + 1) * (high_y - low_y + 1) > 16:
        raise ValueError("at most sixteen requested native pages required")
    requested = {
        (zoom, x, y) for x in range(low_x, high_x + 1) for y in range(low_y, high_y + 1)
    }
    reader, anchor, pages = page_prefix(data)
    selected = {}
    for row, nodes in page_records(reader, pages):
        tile = (row["zoom"], row["x"], row["y"])
        if tile not in requested:
            continue
        if tile in selected:
            raise ValueError("requested native tile has multiple page records")
        if not eligible(row):
            raise ValueError("requested native page layout is unsupported")
        selected[tile] = row, checked_heights(nodes)
    missing = requested - selected.keys()
    if missing:
        raise ValueError(f"requested native tiles missing: {sorted(missing)}")
    return [selected[tile] for tile in sorted(requested)], scope(
        anchor,
        pages,
        "explicit complete native tile window; no generation or ancestor substitution",
    )


def page_fractions(row):
    if not row["virtual"] and row["source_zoom"] < 0:
        postings = row["postings"]
        return (np.arange(33, dtype=np.int64) * (postings - 1) // 32) / (postings - 1)
    return np.arange(33, dtype=np.float64) / 32


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
    np.savetxt(
        args.output / (row["page"] + ".fractions"), page_fractions(row), fmt="%.17g"
    )
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
