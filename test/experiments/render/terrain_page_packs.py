"""Measure independent spatial packages from an existing native ground region."""

import argparse
from collections import defaultdict
from ctypes import c_int, c_size_t, c_uint, c_void_p, create_string_buffer
from hashlib import sha256
import json
from pathlib import Path
from time import perf_counter

from terrain_cached_page import load_zstd, package, page_prefix, page_records


def compressor():
    zstd = load_zstd()
    zstd.ZSTD_compressBound.argtypes = [c_size_t]
    zstd.ZSTD_compressBound.restype = c_size_t
    zstd.ZSTD_compress.argtypes = [c_void_p, c_size_t, c_void_p, c_size_t, c_int]
    zstd.ZSTD_compress.restype = c_size_t
    zstd.ZSTD_isError.argtypes = [c_size_t]
    zstd.ZSTD_isError.restype = c_uint

    def stored_bytes(native):
        encoded = create_string_buffer(zstd.ZSTD_compressBound(len(native)))
        size = zstd.ZSTD_compress(encoded, len(encoded), native, len(native), 1)
        if zstd.ZSTD_isError(size):
            raise ValueError("compression failed")
        return min(len(native), size)

    return stored_bytes


def pages(data):
    reader, _, count = page_prefix(data)
    previous = reader.at
    result = {}
    for row, _ in page_records(reader, count):
        address = row["zoom"], row["x"], row["y"]
        if address in result:
            raise ValueError("ground region has competing page addresses")
        result[address] = memoryview(reader.data)[previous:reader.at]
        previous = reader.at
    return result


def spatial_packs(records, side):
    packs = defaultdict(list)
    for (zoom, x, y), page in sorted(records.items()):
        packs[zoom, x // side, y // side].append(page)
    return packs.values()


def measure(records, side, compress):
    start = perf_counter()
    packs = spatial_packs(records, side)
    sizes = [compress(b"".join(batch)) for batch in packs]
    return {"side": side, "packs": len(packs), "stored_bytes": sum(sizes),
            "max_read_bytes": max(sizes), "max_pages": max(map(len, packs)),
            "preparation_ms": (perf_counter() - start) * 1000}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("keys", nargs="+")
    args = parser.parse_args()
    compress = compressor()
    referenced_packs = 0
    unique_packs = set()
    for key in args.keys:
        data, provenance = package(args.cache, key)
        records = pages(data)
        print(json.dumps({"region": provenance, "pages": len(records),
                          "page_native_bytes": sum(map(len, records.values())),
                          "monolithic_page_bytes": compress(b"".join(records.values())),
                          "spatial_packs": [measure(records, side, compress)
                                            for side in (1, 2, 4, 8, 16)]}))
        packs = spatial_packs(records, 8)
        referenced_packs += len(packs)
        unique_packs.update(sha256(b"".join(batch)).digest() for batch in packs)
    print(json.dumps({"pack_side": 8, "region_pack_references": referenced_packs,
                      "distinct_pack_payloads": len(unique_packs)}))


if __name__ == "__main__":
    main()
