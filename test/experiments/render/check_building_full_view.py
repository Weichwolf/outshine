"""Check a full native view against its unchanged original artifact bytes."""

import argparse
import gzip
import hashlib
import json
import struct
from pathlib import Path

from check_building_capture_inputs import expanded_stream, require


def packed_vertices(stream):
    return b"".join(
        struct.pack("<5fI", *position, *uv, normal)
        for position, uv, normal in zip(
            stream["positions"], stream["uv"], stream["normal_words"], strict=True
        )
    )


def packed(values, code):
    return struct.pack("<" + str(len(values)) + code, *values)


def check(report, original):
    magic = b"outshine-structure"
    require(len(original) <= 64 * 1024**2, "original artifact exceeds native bound")
    require(original.startswith(magic), "invalid native artifact magic")
    require(
        struct.unpack_from("<I", original, len(magic))[0] == 10, "unsupported artifact"
    )
    require(
        hashlib.sha256(original[:-64]).hexdigest().encode() == original[-64:],
        "original artifact SHA mismatch",
    )
    at = len(magic) + 4
    require(
        original[at : at + 64].decode() == report["view_key"], "view identity mismatch"
    )
    at += 64
    require(len(report["results"]) == 1, "one original complete view required")
    result = report["results"][0]
    geometry = result["geometry"]
    require(
        result["accepted"] and result["archived_geometry_identical"],
        "native replay failed",
    )
    streams = {name: expanded_stream(geometry, name) for name in ("walls", "roofs")}
    expected = [
        packed_vertices(geometry["walls"]),
        packed_vertices(geometry["roofs"]),
        packed([i for t in geometry["walls"]["triangles"] for i in t], "I"),
        packed([i for t in geometry["roofs"]["triangles"] for i in t], "I"),
        None,
        packed(geometry["wall_colours"], "f"),
        packed(geometry["wall_material_metres"], "f"),
        None,
        None,
        None,
    ]
    for size, want in zip((24, 24, 4, 4, 52, 4, 4, 4, 52, 4), expected, strict=True):
        require(at + 8 <= len(original) - 64, "truncated native list")
        count = struct.unpack_from("<Q", original, at)[0]
        at += 8
        end = at + count * size
        require(end <= len(original) - 64, "truncated native payload")
        require(
            want is None or original[at:end] == want, "native geometry bytes differ"
        )
        at = end
    sources = {source["index"] for source in report["sources"]}
    require(len(sources) == len(report["sources"]), "duplicate source index")
    used = set()
    ends = {"wall": 0, "roof": 0}
    for call in result["mesh_calls"]:
        source = call["source"]
        require(call["accepted"], "source meshing failed")
        require(source is None or source in sources, "unbound source")
        require(source is None or source not in used, "duplicate individual source")
        if source is not None:
            used.add(source)
        for name, previous in ends.items():
            first, count = call[name + "_index_range"]
            require(
                first == previous and count >= 0 and count % 3 == 0,
                "invalid source range",
            )
            ends[name] = first + count
    for name, end in ends.items():
        require(
            end == 3 * len(geometry[name + "s"]["triangles"]),
            "incomplete source ranges",
        )
    if all(detail < 2 for detail in result["details"]):
        require(used == sources, "individual source omitted")
    return {
        "sources": len(sources),
        "triangles": sum(ends.values()) // 3,
        "expanded_bytes": sum(map(len, streams.values())),
    }


def read(path):
    return (
        gzip.decompress(path.read_bytes())
        if path.suffix == ".gz"
        else path.read_bytes()
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--artifact", type=Path, required=True)
    args = parser.parse_args()
    print(
        json.dumps(
            check(json.loads(read(args.input)), read(args.artifact)), sort_keys=True
        )
    )
