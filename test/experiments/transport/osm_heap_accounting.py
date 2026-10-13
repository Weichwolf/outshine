"""Compare repeated string accounting with immutable tile summaries on cached MVTs.

UTF-8 byte counts isolate accounting work, not C++ capacities or OS resident memory.
"""
import argparse
import json
import time
from pathlib import Path


def varint(data, at):
    value = 0
    for shift in range(0, 70, 7):
        byte = data[at]
        at += 1
        value |= (byte & 127) << shift
        if byte < 128:
            return value, at
    raise ValueError("invalid varint")


def fields(data):
    at = 0
    while at < len(data):
        key, at = varint(data, at)
        number, wire = key >> 3, key & 7
        if wire == 0:
            _, at = varint(data, at)
        elif wire in (1, 5):
            at += 8 if wire == 1 else 4
        elif wire == 2:
            size, at = varint(data, at)
            stop = at + size
            if stop > len(data):
                raise ValueError("truncated field")
            yield number, data[at:stop]
            at = stop
        else:
            raise ValueError("unsupported wire type")


def strings(data):
    result = []
    for number, layer in fields(data):
        if number != 3:
            continue
        for kind, value in fields(layer):
            if kind == 3:
                result.append(value)
            elif kind == 4:
                result.extend(text for tag, text in fields(value) if tag == 1)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("cache", type=Path)
    parser.add_argument("--tiles", type=int, default=16)
    parser.add_argument("--queries", type=int, default=200)
    args = parser.parse_args()
    tiles = []
    for path in args.cache.iterdir():
        if not path.is_file():
            continue
        with path.open("rb") as handle:
            if handle.read(1) != b"\x1a":
                continue
        values = strings(path.read_bytes())
        if values:
            tiles.append(values)
        if len(tiles) == args.tiles:
            break
    assert len(tiles) == args.tiles
    start = time.perf_counter()
    repeated = [sum(len(value) for tile in tiles for value in tile)
                for _ in range(args.queries)]
    repeated_s = time.perf_counter() - start
    start = time.perf_counter()
    summaries = [sum(map(len, tile)) for tile in tiles]
    cached = [sum(summaries) for _ in range(args.queries)]
    cached_s = time.perf_counter() - start
    assert repeated == cached
    live = dict(enumerate(tiles))
    charges = dict(enumerate(summaries))
    for slot, replacement in ((0, tiles[-1] + [b"replacement"]), (1, None)):
        if replacement is None:
            del live[slot]
            del charges[slot]
        else:
            live[slot] = replacement
            charges[slot] = sum(map(len, replacement))
        assert sum(charges.values()) == sum(len(value) for tile in live.values() for value in tile)
    print(json.dumps(dict(tiles=len(tiles), strings=sum(map(len, tiles)), queries=args.queries,
                          repeated_s=repeated_s, summarized_s=cached_s,
                          string_visits_before=sum(map(len, tiles)) * args.queries,
                          string_visits_after=sum(map(len, tiles)),
                          measurement="Python accounting only; not C++ time, capacity or RSS")))


if __name__ == "__main__":
    main()
