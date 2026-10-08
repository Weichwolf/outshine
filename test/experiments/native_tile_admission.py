"""Compare feature scans and metadata admission using real native building counts."""
import argparse
import json
from pathlib import Path
import sqlite3
import statistics
import time

from building_asset_delivery import library
from prepared_building_residency import Reader, unpack


def count_sources(data):
    reader = Reader(data)
    assert (reader.number("I"), reader.number("I")) == (0x31425350, 5)
    reader.skip(24 + 8 + 4 + 1 + 8 + 4 + 32 + 8)
    if reader.flag():
        reader.text()
        reader.text()
        for _ in range(reader.number("Q")):
            reader.text()
        if reader.flag():
            reader.skip(12)
    for _ in range(reader.number("Q")):
        reader.skip(4 + 1 + 12)
        if reader.flag():
            reader.skip(8)
        reader.text()
        reader.text()
    reader.skip(8 + 1 + 4 + 1)
    reader.fixed_list(12)
    reader.fixed_list(8)
    reader.fixed_list(9)
    count = reader.fixed_list(13)
    assert reader.at == len(data)
    return count


def feature_scan(features, taken):
    result = []
    at = 0
    while at < len(features):
        first = at
        tile = features[at]
        while at < len(features) and features[at] == tile:
            at += 1
        if tile not in taken:
            result.append((tile, first, at))
    return result


def metadata_scan(tiles, taken):
    return [(tile, first, first + count) for tile, first, count in tiles
            if count and tile not in taken]


def measure(operation):
    samples = []
    for _ in range(7):
        started = time.perf_counter()
        operation()
        samples.append((time.perf_counter() - started) * 1000)
    return statistics.median(samples)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, default=Path.home() /
                        "Library/Application Support/outshine/outshine/assets/assets.sqlite")
    args = parser.parse_args()
    lib = library()
    tiles, features = [], []
    with sqlite3.connect(args.cache.resolve().as_uri() + "?mode=ro", uri=True) as database:
        sql = """SELECT a.id,p.codec,p.native_bytes,p.bytes FROM assets a
                 JOIN packages p ON p.key=a.package WHERE a.kind='building-basis'
                 ORDER BY a.id"""
        for tile, codec, size, wire in database.execute(sql):
            count = count_sources(unpack(lib, wire, codec, size))
            tiles.append((tile, len(features), count))
            features.extend([tile] * count)
    assert tiles and features
    reports = []
    for divisor in (0, 2, 7):
        taken = {tile for tile, _, _ in tiles if divisor and tile % divisor == 0}
        expected = feature_scan(features, taken)
        assert metadata_scan(tiles, taken) == expected
        reports.append(dict(taken_tiles=len(taken), candidates=len(expected),
                            feature_scan_ms=measure(lambda: feature_scan(features, taken)),
                            metadata_scan_ms=measure(lambda: metadata_scan(tiles, taken))))
    print(json.dumps(dict(native_tiles=len(tiles), native_structures=len(features),
                          scope="native counts model feature runs; ordering/radius unchanged; "
                                "no runtime, visibility or source-bypass proof", cases=reports), indent=2))


if __name__ == "__main__":
    main()
