"""Check native unprojected cells against independently decoded original source records."""

import argparse
import gzip
import json
from pathlib import Path

from check_building_capture_inputs import expanded_stream, require


def check(report, records, parent):
    cell = report["view"]["cell"]
    expected = [(i, r) for i, r in enumerate(records) if r["cell"] == cell]
    sources = report["sources"]
    require(report["parent"] == parent and expected, "original parent/cell mismatch")
    require(len(sources) == len(expected), "incomplete original cell")
    for source, (index, original) in zip(sources, expected, strict=True):
        require(
            source["index"] == index
            and source["cell"] == cell
            and (source["kind"], source["id"]) == tuple(original["source_id"]),
            "original source identity/order mismatch",
        )
    require(len(report["results"]) == 1, "one native cell required")
    result = report["results"][0]
    require(result["accepted"] and result["cell"] == cell, "native cell failed")
    require(not result["projection_calls"], "projected geometry is not a 3D control")
    require(
        len(result["details"]) == result["footprints"] == len(sources)
        and all(d in (0, 1) for d in result["details"]),
        "individually identified Fine/Shell controls required",
    )
    geometry = result["geometry"]
    streams = {n: expanded_stream(geometry, n) for n in ("walls", "roofs")}
    selected = {s["index"] for s in sources}
    seen, failures = set(), []
    ends = {"wall": 0, "roof": 0}
    for call in result["mesh_calls"]:
        source = call["source"]
        require(
            source in selected and source not in seen, "unbound/duplicate native source"
        )
        seen.add(source)
        for name, end in ends.items():
            first, count = call[name + "_index_range"]
            require(
                first == end and type(count) is int and count >= 0 and count % 3 == 0,
                "invalid native source range",
            )
            require(call["accepted"] or count == 0, "failed source leaked geometry")
            ends[name] += count
        if not call["accepted"]:
            require(
                isinstance(call["error"], str) and call["error"],
                "missing native failure reason",
            )
            failures.append({"source": source, "reason": call["error"]})
    require(seen == selected, "original source silently omitted")
    require(
        result["unsupported"] == len(failures),
        "unsupported count differs from failures",
    )
    for name, end in ends.items():
        require(
            end == 3 * len(geometry[name + "s"]["triangles"]),
            "incomplete geometry ranges",
        )
    triangles = sum(ends.values()) // 3
    require(result["fallback_triangles"] == triangles, "native triangle count mismatch")
    return {
        "cell": cell,
        "sources": len(sources),
        "triangles": triangles,
        "expanded_bytes": sum(map(len, streams.values())),
        "unsupported": failures,
    }


if __name__ == "__main__":
    from parent_surface_inputs import open_parent

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--input", required=True, type=Path)
    args = parser.parse_args()
    raw = args.input.read_bytes()
    report = json.loads(gzip.decompress(raw) if args.input.suffix == ".gz" else raw)
    with open_parent(args.source) as original:
        print(json.dumps(check(report, original.plans["records"], original.parent)))
