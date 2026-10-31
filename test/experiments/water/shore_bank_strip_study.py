"""Construct a finite inferred bank solid at current native water and road interfaces.

This is a local recipe candidate, not a native terrain replacement or World claim.
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
from native_water_prepared import bind_export, decode
from outshine_ref.contracts import Mesh
from outshine_ref.geometry.sampling import SurfaceSampler
from shore_bank_inputs import (
    height,
    road_chain,
    road_rim,
    roads,
    sample_roads,
    source_dem,
)
from shore_bank_strip import build
from shore_road_sources import interfaces
from water_shore_component import component

FIXTURE = Path(__file__).parents[1] / "transport/fixtures/feldkirch-shore-contact"


def run(args):
    if args.output.exists():
        raise ValueError("fresh explicit output required")
    receipt = runpy.run_path(str(FIXTURE / "verify.py"))["verify"](args.inputs)
    manifest = json.loads((args.inputs / "input.json").read_bytes())
    capture = json.loads(
        gzip.decompress((args.inputs / "contact.json.gz").read_bytes())
    )
    data = decode(gzip.decompress((args.inputs / "water.native.gz").read_bytes()))
    bind_export(data, capture["water"])
    whole = component(data["prepared"], args.face)
    domain = shapely.box(*args.bounds)
    selector = getattr(args, "shore_point", None)
    if selector is None:
        selector = manifest["native_observation"]["points_enu_m"][1]
    witness = shapely.Point(selector)
    loops = [
        shapely.LineString(whole["vertices"][np.r_[ids, ids[0]], :2])
        for a, b in pairwise(whole["loop_offsets"])
        for ids in [whole["loop_vertices"][a:b]]
    ]
    local = shapely.get_parts(
        shapely.intersection(shapely.multilinestrings(loops), domain)
    )
    shore = min(local, key=lambda g: g.distance(witness))
    if shore.geom_type != "LineString" or not shore.is_simple or shore.is_closed:
        raise ValueError("one open simple local bank interface required")
    expanded = np.array(args.bounds) + [-10, -10, 10, 10]
    triangles, face_ids, plan_key = roads(args.inputs, whole["anchor"], expanded)
    road_xy, road_domain = road_chain(shore, triangles)
    if not shapely.box(*expanded).buffer(-2).contains(shapely.LineString(road_xy)):
        raise ValueError("road selection frontier cannot become a physical interface")
    road, bindings = sample_roads(triangles, road_xy)
    shore_xy = shapely.get_coordinates(shore)
    sampler = SurfaceSampler(Mesh(whole["vertices"], whole["faces"]))
    water = np.c_[shore_xy, height(sampler, shore_xy)]
    rim_source = getattr(args, "rim_source", "source_dem")
    if rim_source == "source_dem":
        prior = source_dem(args.inputs, whole["anchor"], manifest, expanded)
        rim = height(prior, shore_xy)
    elif rim_source == "road_level":
        rim = road_rim(road, shore_xy)
    else:
        raise ValueError("explicit source_dem or road_level bank rim required")
    tracemalloc.start()
    started = time.process_time()
    body = build(water, rim, road, bed_depth_m=args.depth)
    elapsed = time.process_time() - started
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    roof_polygons = shapely.polygons(body["vertices"][body["roof_faces"], :2])
    footprint = shapely.coverage_union_all(roof_polygons)
    water_domain = shapely.union_all(
        shapely.polygons(whole["vertices"][whole["faces"], :2])
    )
    intrusion = [footprint.intersection(g).area for g in (water_domain, road_domain)]
    if max(intrusion) > 1e-7 or not footprint.covers(witness):
        raise ValueError("strip crosses protected footprints or misses the actual gap")
    # Check every source-facet breakpoint along the entire actual road interface.
    chain = shapely.LineString(road_xy)
    breaks = shapely.get_coordinates(
        shapely.intersection(
            chain, shapely.boundary(shapely.polygons(triangles[:, :, :2]))
        )
    )
    exact_road, break_bindings = sample_roads(triangles, breaks)
    stations = np.r_[0, np.cumsum(np.linalg.norm(np.diff(road_xy, axis=0), axis=1))]
    target = np.interp(
        shapely.line_locate_point(chain, shapely.points(breaks)), stations, road[:, 2]
    )
    error = float(np.max(abs(exact_road[:, 2] - target)))
    if error > 1e-7:
        raise ValueError("road interface crosses an unresolved source-height crease")
    args.output.mkdir(parents=True)
    np.savez(args.output / "bank.npz", **body, water=water, road=road, dry_rim_up=rim)
    np.savez(args.output / "water-component.npz", **whole)
    bindings = [
        [face_ids[i].tolist() for i in values] for values in bindings + break_bindings
    ]
    bound = interfaces(args.inputs, bindings, plan_key)
    bound["road_breakpoint_faces"] = bound["node_faces"][len(road) :]
    bound["node_faces"] = bound["node_faces"][: len(road)]
    (args.output / "road-bindings.json").write_text(json.dumps(bound, indent=2) + "\n")
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "manifest_sha256": hashlib.sha256(
            (args.inputs / "input.json").read_bytes()
        ).hexdigest(),
        "input_receipt": receipt,
        "bounds_enu_m": args.bounds,
        "selector_point_enu_m": list(selector),
        "shore_interface_nodes": len(water),
        "shore_length_m": shore.length,
        "road_interface_nodes": len(road),
        "dry_rim_source": rim_source,
        "road_source_breakpoints_checked": len(breaks),
        "road_interface_max_height_error_m": error,
        "wet_and_road_intrusion_area_m2": intrusion,
        "water_to_dry_rim_up_range_m": [
            float(min(rim - water[:, 2])),
            float(max(rim - water[:, 2])),
        ],
        "vertices": len(body["vertices"]),
        "faces": len(body["faces"]),
        "body_array_bytes": sum(p.nbytes for p in body.values()),
        "indexed_position_face_bytes": body["vertices"].nbytes + body["faces"].nbytes,
        "equivalent_float64_triangle_soup_bytes": len(body["faces"]) * 9 * 8,
        "single_instrumented_body_cpu_s": elapsed,
        "body_traced_peak_bytes": peak,
        "scope": __doc__
        + " The explicit rim choice uses original DEM reference triangles or procedurally continues the supplied road-edge level. It neither reconstructs an observed bank nor invents missing exterior DEM samples. DEM reference triangles are not native bilinear/shader equivalence. Lower exterior closure continues nearest shore bed levels procedurally. Both end caps are continuation ports. Primitive/source membership uses captured ranges; absent exterior source-way rows stay null, junction membership stays shared. Exterior terrain reconciliation, navigation/collision integration, global refusal, source-cut admission and native/device checks remain open. GEOS/Float64 checks are finite, not exact certificates. Costs exclude input preparation, decoding, projections, validation, IO, GEOS native heap and allocation traffic.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    display(body, water, road, triangles, args.output)
    print(json.dumps(report))


def display(body, water, road, triangles, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    figure = plt.figure(figsize=(12, 6), layout="constrained")
    for index, elevation in enumerate((22, 65), 1):
        axis = figure.add_subplot(1, 2, index, projection="3d")
        axis.add_collection3d(
            Poly3DCollection(
                body["vertices"][body["faces"]],
                facecolor="tan",
                edgecolor="saddlebrown",
                linewidth=0.2,
            )
        )
        axis.add_collection3d(
            Poly3DCollection(triangles, facecolor="gray", alpha=0.35, linewidth=0)
        )
        axis.plot(*water.T, color="blue", linewidth=2)
        axis.plot(*road.T, color="black", linewidth=1)
        points = body["vertices"]
        low, high = points.min(axis=0), points.max(axis=0)
        axis.set(
            xlim=(low[0] - 2, high[0] + 2),
            ylim=(low[1] - 2, high[1] + 2),
            zlim=(low[2] - 1, high[2] + 1),
        )
        axis.set_box_aspect(high - low)
        axis.view_init(elev=elevation, azim=-125)
    figure.suptitle(
        "Inferred local retaining strip; fixed native water/road interfaces, open terrain continuation"
    )
    figure.savefig(output / "bank.png", dpi=140)
    plt.close(figure)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--face", type=int, required=True)
    parser.add_argument("--bounds", type=float, nargs=4, required=True)
    parser.add_argument("--depth", type=float, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--rim-source", choices=("source_dem", "road_level"), default="source_dem"
    )
    parser.add_argument("--shore-point", type=float, nargs=2)
    run(parser.parse_args())
