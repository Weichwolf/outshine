"""Inspect the frozen native GEOS source/union membership counterexample."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path

import numpy as np
import shapely

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
DEFAULT = HERE / "fixtures/basel-water-source-membership.npz"


def inspect(path):
    metadata = json.loads(path.with_suffix(".json").read_text())
    if (
        hashlib.sha256(path.read_bytes()).hexdigest()
        != metadata["geometry_fixture_sha256"]
    ):
        raise ValueError("geometry fixture differs from the frozen input")
    packed = path.with_suffix(".native.gz").read_bytes()
    if hashlib.sha256(packed).hexdigest() != metadata["compressed_input_sha256"]:
        raise ValueError("compressed original differs from the frozen input")
    native = gzip.decompress(packed)
    if hashlib.sha256(native).hexdigest() != metadata["native_input_sha256"]:
        raise ValueError("native source payload differs from the frozen input")
    with np.load(path, allow_pickle=False) as data:
        sources = shapely.get_parts(shapely.from_wkb(data["sources_wkb"].tobytes()))
        original_union = shapely.from_wkb(data["union_wkb"].tobytes())
        triangle = shapely.from_wkb(data["triangle_wkb"].tobytes())
        ids = data["source_indices"]
        if len(sources) != len(ids):
            raise ValueError("source identities and captured polygons differ")
        areas = shapely.area(shapely.intersection(sources, triangle))
        coordinates = shapely.get_coordinates(triangle)[:-1]
        edges = np.linalg.norm(np.roll(coordinates, -1, axis=0) - coordinates, axis=1)
        return {
            "geos": shapely.geos_version_string,
            "sources": len(sources),
            "positive_source_overlaps": int((areas > 0).sum()),
            "source_overlap_area_sum_m2": float(areas.sum()),
            "native_union_overlap_m2": float(
                shapely.area(shapely.intersection(original_union, triangle))
            ),
            "rebuilt_union_overlap_m2": float(
                shapely.area(shapely.intersection(shapely.union_all(sources), triangle))
            ),
            "triangle_area_m2": float(shapely.area(triangle)),
            "shortest_edge_m": float(edges.min()),
            "native_input_sha256": metadata["native_input_sha256"],
            "scope": "Unmodified geometry diagnosis; no repair, model admission or world/device claim.",
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT):
        parser.error("private results belong outside the checkout")
    report = inspect(args.input)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
