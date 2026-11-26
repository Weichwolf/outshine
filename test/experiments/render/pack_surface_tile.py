"""Pack an exported surface tile as one independently readable render product."""

import argparse
import json
from pathlib import Path
import struct


HEADER = struct.Struct("<8sIII2d64s7dII")
VERTEX = struct.Struct("<6f")
INDEX = struct.Struct("<I")


def pack(tile):
    positions, normals, indices = (tile[field] for field in (
        "vertices_eus_m", "normals_eus", "indices",
    ))
    revision = tile["revision"].encode("ascii")
    if tile["version"] != 1 or len(revision) != 64 or len(positions) != len(normals):
        raise ValueError("invalid version-one surface tile")
    minimum, maximum = tile["bounds_eus_m"]
    payload = bytearray(HEADER.pack(
        b"OUTSHST1", *tile["coverage"], *tile["origin_lon_lat"], revision, *minimum, *maximum,
        tile["geometric_error_m"], len(positions), len(indices),
    ))
    for position, normal in zip(positions, normals):
        payload += VERTEX.pack(*position, *normal)
    for index in indices:
        payload += INDEX.pack(index)
    return payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("product", type=Path)
    options = parser.parse_args()
    payload = pack(json.loads(options.source.read_bytes()))
    with options.product.open("xb") as output:
        output.write(payload)


if __name__ == "__main__":
    main()
