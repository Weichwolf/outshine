"""Pack an exported surface tile as one independently readable render product."""

import argparse
import hashlib
import json
from pathlib import Path
import struct


HEADER = struct.Struct("<8sIII2d64s7dII")
VERTEX = struct.Struct("<6f")
INDEX = struct.Struct("<I")
COLOUR = struct.Struct("<4B")


def pack(tile):
    positions, normals, indices = (tile[field] for field in (
        "vertices_eus_m", "normals_eus", "indices",
    ))
    revision = tile["revision"].encode("ascii")
    if tile["version"] != 1 or len(revision) != 64 or len(positions) != len(normals):
        raise ValueError("invalid version-one surface tile")
    minimum, maximum = tile["bounds_eus_m"]
    grid_side = tile.get("grid_side", 0)
    colours = tile.get("colours_rgba8")
    magic = b"OUTSHST2" if grid_side else b"OUTSHST1"
    if colours is not None:
        if not grid_side or len(colours) not in (len(positions), len(positions) - grid_side**2):
            raise ValueError("material colours must cover the overlay or all vertices")
        magic = b"OUTSHST4" if len(colours) == len(positions) else b"OUTSHST3"
    if tile.get("ground_roughness", False):
        if magic != b"OUTSHST4":
            raise ValueError("ground roughness requires a complete material stream")
        magic = b"OUTSHST5"
    payload = bytearray(HEADER.pack(
        magic,
        *tile["coverage"], *tile["origin_lon_lat"], revision, *minimum, *maximum,
        tile["geometric_error_m"], len(positions), len(indices),
    ))
    if grid_side:
        payload += INDEX.pack(grid_side)
    if colours is not None:
        payload += INDEX.pack(tile["ground_indices"])
    for position, normal in zip(positions, normals):
        payload += VERTEX.pack(*position, *normal)
    for index in indices:
        payload += INDEX.pack(index)
    if colours is not None:
        for colour in colours:
            payload += COLOUR.pack(*colour)
    return payload


def with_ground_material(packed, ground):
    header = list(HEADER.unpack_from(packed))
    magic, vertices, indices = header[0], header[-2], header[-1]
    if magic not in (b"OUTSHST2", b"OUTSHST3"):
        raise ValueError("packed regular surface grid required")
    side = INDEX.unpack_from(packed, HEADER.size)[0]
    grid = side * side
    split = INDEX.size if magic == b"OUTSHST3" else 0
    end = HEADER.size + INDEX.size + split + vertices * VERTEX.size + indices * INDEX.size
    if side < 2 or grid > vertices or (not split and grid != vertices):
        raise ValueError("invalid ground grid size")
    if len(ground) != grid * COLOUR.size or len(packed) != end + (vertices - grid) * COLOUR.size:
        raise ValueError("material colour payload does not match vertex counts")
    header[0] = b"OUTSHST5"
    header[6] = hashlib.sha256(packed + ground).hexdigest().encode("ascii")
    body = packed[HEADER.size:end]
    if not split:
        body = body[:INDEX.size] + INDEX.pack(indices) + body[INDEX.size:]
    return HEADER.pack(*header) + body + ground + packed[end:]


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
