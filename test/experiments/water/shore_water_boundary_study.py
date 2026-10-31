"""Recover the whole current native water component and its source-supported loops.

Native mesh boundaries are retained, not automatically admitted as physical banks.
"""

import argparse
import gzip
import hashlib
import json
import runpy
import subprocess
import time
import tracemalloc
from itertools import pairwise
from pathlib import Path

import numpy as np
import shapely
from native_river_inputs import surface_rings
from native_water_prepared import bind_export, decode
from outshine_ref.water.geodetic import local_points
from water_shore_component import component

FIXTURE = Path(__file__).parents[1] / "transport/fixtures/feldkirch-shore-contact"


def run(args):
    if args.output.exists():
        raise ValueError("Fresh output required")
    receipt = runpy.run_path(str(FIXTURE / "verify.py"))["verify"](args.inputs)
    metadata_raw = (args.inputs / "input.json").read_bytes()
    with gzip.open(args.inputs / "contact.json.gz", "rt") as stream:
        capture = json.load(stream)
    raw = gzip.decompress((args.inputs / "water.native.gz").read_bytes())
    data = decode(raw)
    bind_export(data, capture["water"])
    tracemalloc.start()
    began = time.process_time()
    body = component(data["prepared"], args.face)
    elapsed = time.process_time() - began
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    sources = np.unique(body["face_sources"])
    polygons, source_keys = [], []
    for identity in sources:
        surface = data["surfaces"][identity]
        info = data["inputs"][int(surface["input"])]
        source = data["sources"][info["source"]]
        if (
            info["kind"] != "river"
            or info["bridge"]
            or info["tunnel"]
            or info["crossing"]
        ):
            raise ValueError("This bounded ground-river comparison is unqualified")
        rings = [
            local_points(body["anchor"], ring[:, ::-1], np.zeros(len(ring)))[:, :2]
            for ring in surface_rings(data, int(identity))
        ]
        polygons.append(shapely.Polygon(rings[0], rings[1:]))
        source_keys.append({"surface": int(identity), "input": info, "source": source})
    original = shapely.union_all(polygons)
    projected = shapely.union_all(shapely.polygons(body["vertices"][body["faces"], :2]))
    args.output.mkdir(parents=True)
    np.savez(args.output / "component.npz", **body)
    (args.output / "source-lineage.json").write_text(
        json.dumps(source_keys, indent=2) + "\n"
    )
    report = {
        "producer_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "producer_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                Path(__file__),
                Path(__file__).with_name("native_water_prepared.py"),
                Path(__file__).with_name("water_shore_component.py"),
            )
        },
        "manifest_sha256": hashlib.sha256(metadata_raw).hexdigest(),
        "native_water_sha256": hashlib.sha256(raw).hexdigest(),
        "input_receipt": receipt,
        "seed_face": args.face,
        "component_faces": len(body["faces"]),
        "component_vertices": len(body["vertices"]),
        "source_surfaces": sources.tolist(),
        "boundary_loops": len(body["loop_offsets"]) - 1,
        "boundary_vertices": len(body["loop_vertices"]),
        "component_array_bytes": sum(a.nbytes for a in body.values()),
        "single_instrumented_component_cpu_s": elapsed,
        "component_traced_peak_bytes": peak,
        "source_union_area_m2": original.area,
        "native_component_area_m2": projected.area,
        "source_difference_area_m2": original.symmetric_difference(projected).area,
        "source_boundary_discrete_hausdorff_m": float(
            shapely.hausdorff_distance(original.boundary, projected.boundary)
        ),
        "component_sha256": hashlib.sha256(
            (args.output / "component.npz").read_bytes()
        ).hexdigest(),
        "scope": __doc__
        + " Source polygons use the existing WGS84 ground-height projection reference. Finite GEOS comparisons are not an exact/native Float32 certificate. Costs cover component extraction from resident decoded inputs only; source projection, IO, decoding, GEOS unions, serialization, native/OS memory and allocation traffic are excluded. Source/capture cuts and road/nav/exterior closure remain open. No contact repair or World/device admission.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    plot(body, original, capture, json.loads(metadata_raw), args.output)
    print(json.dumps(report))


def plot(body, original, capture, metadata, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axes = plt.subplots(1, 2, figsize=(11, 5), layout="constrained")
    for axis in axes:
        for polygon in shapely.get_parts(original):
            axis.plot(
                *polygon.exterior.xy, color="black", linewidth=1, label="Source union"
            )
            for hole in polygon.interiors:
                axis.plot(*hole.xy, color="black", linewidth=1)
        offsets = body["loop_offsets"]
        for a, b in pairwise(offsets):
            ids = body["loop_vertices"][a:b]
            points = body["vertices"][np.r_[ids, ids[0]], :2]
            axis.plot(points[:, 0], points[:, 1], color="blue", linewidth=0.5)
        axis.set_aspect("equal")
    points = np.array(metadata["native_observation"]["points_enu_m"])
    centre = points.mean(axis=0)
    axes[1].scatter(points[:, 0], points[:, 1], c=["red", "green"])
    axes[1].set(
        xlim=(centre[0] - 15, centre[0] + 15), ylim=(centre[1] - 15, centre[1] + 15)
    )
    axes[0].set_title("Whole native component / source union")
    axes[1].set_title("Current boundary at the reported points")
    figure.suptitle("Native water lineage and closed loops; not a repaired contact")
    figure.savefig(output / "boundary.png", dpi=130)
    plt.close(figure)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--face", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
