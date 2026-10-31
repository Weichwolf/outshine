"""Conform the complete captured terrain window to a fixed inferred bank body.

Outer-edge changes remain explicit obligations; no native/World admission.
"""

import argparse
import json
import os
import platform
import subprocess
import time
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import shapely
from allocation_measure import allocations
from conforming_body_contact import contact_error
from outshine_ref.contracts import Mesh
from outshine_ref.geometry.sampling import SurfaceSampler
from pool_terrain_contact import ContactSurface
from shore_bank_strip_study import run as bank_study
from shore_contact_audit import audit, boundary_edges
from shore_contact_edges import attach
from shore_native_terrain import load
from shore_terrain_cut import cut


def reconcile(body, vertices, faces, footprint, *, method="local"):
    before, terrain_faces, sources, cut_note = cut(
        vertices, faces, footprint, method=method
    )
    outer = shapely.union_all(shapely.linestrings(vertices[boundary_edges(faces), :2]))
    terrain, refined, refined_faces, contacts, parents, edge_note = attach(
        body, before, terrain_faces, outer
    )
    interface = audit(
        body,
        vertices,
        faces,
        before,
        terrain,
        terrain_faces,
        refined,
        refined_faces,
        parents,
        contacts,
    )
    return {
        "terrain_vertices": terrain,
        "terrain_faces": terrain_faces,
        "terrain_source_faces": sources,
        "body_vertices": refined,
        "body_faces": refined_faces,
        "body_parent_faces": parents,
        "contacts": contacts,
    }, {"cut": cut_note, "edge_subdivision": edge_note, "interfaces": interface}


def canonical_faces(products):
    faces = products["terrain_faces"]
    rotations = (faces.argmin(axis=1)[:, None] + np.arange(3)) % 3
    faces = np.take_along_axis(faces, rotations, axis=1)
    rows = np.c_[products["terrain_source_faces"], faces]
    return rows[np.lexsort(rows[:, ::-1].T)]


def water_bed_comparison(water, original, original_faces, products, depth_m):
    bed = ContactSurface(
        SurfaceSampler(Mesh(water["vertices"] - [0, 0, depth_m], water["faces"]))
    )
    return {
        "original": contact_error(bed, original, original_faces),
        "after": contact_error(
            bed, products["terrain_vertices"], products["terrain_faces"]
        ),
        "declared_bed_depth_m": depth_m,
        "scope": "Independent affine common-refinement comparison over the whole water/terrain overlap in the supplied window; target is the captured native water minus the caller's declared bed offset. This validation is outside the timed/profiled construction. The whole river and unknown exterior are not tested.",
    }


def measure(body, vertices, faces, footprint, output, *, profile):
    results, times = {}, {"global": [], "local": []}
    for round_id in range(3):
        order = ("global", "local") if round_id % 2 == 0 else ("local", "global")
        for method in order:
            started = time.process_time()
            results[method] = reconcile(body, vertices, faces, footprint, method=method)
            times[method].append(time.process_time() - started)
    first, second = results["global"][0], results["local"][0]
    for key in first:
        if (
            key not in ("terrain_faces", "terrain_source_faces")
            and first[key].tobytes() != second[key].tobytes()
        ):
            raise ValueError("local selection changes complete output words: " + key)
    if canonical_faces(first).tobytes() != canonical_faces(second).tobytes():
        raise ValueError("local selection changes exterior facets or source ownership")
    profiles = {}
    if profile:
        for method in ("global", "local"):
            _, profiles[method] = allocations(
                output / (method + ".memray"),
                lambda method=method: reconcile(
                    body, vertices, faces, footprint, method=method
                ),
            )
    return results["local"], {
        "normal_process_cpu_s": times,
        "allocations": profiles,
        "equivalent_geometry_and_source_words": True,
    }


def run(args):
    if args.output.exists():
        raise ValueError("fresh explicit system-temp output required")
    args.output.mkdir(parents=True)
    # Reproduce the current bank and its source bindings in this same process.
    bank_study(SimpleNamespace(**(vars(args) | {"output": args.output / "bank"})))
    with np.load(args.output / "bank/bank.npz", allow_pickle=False) as saved:
        body = dict(saved)
    with np.load(args.output / "bank/water-component.npz", allow_pickle=False) as saved:
        water = dict(saved)
    manifest = json.loads((args.inputs / "input.json").read_bytes())
    original, faces, heights = load(args.inputs, water["anchor"], manifest)
    footprint = shapely.coverage_union_all(
        shapely.polygons(body["vertices"][body["roof_faces"], :2])
    )
    load_before = os.getloadavg()
    (products, note), costs = measure(
        body, original, faces, footprint, args.output, profile=args.allocations
    )
    np.savez(
        args.output / "contact.npz",
        **products,
        original_vertices=original,
        original_faces=faces,
        original_height_words=heights,
    )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "host": platform.platform(),
        "logical_cpus": os.cpu_count(),
        "load_average_before": load_before,
        "load_average_after": os.getloadavg(),
        "bank_receipt": json.loads((args.output / "bank/report.json").read_bytes()),
        "geometry": note,
        "water_bed_comparison": water_bed_comparison(
            water, original, faces, products, args.depth
        ),
        "costs": costs,
        "held_output_arrays_bytes": sum(v.nbytes for v in products.values()),
        "scope": __doc__
        + " Existing Float32 postings use the declared WGS84 Double projection and NW-SE diagonals, not native vertex/shader equivalence. Terrain and the closed body are separate meshes, not a Boolean union of solids. All GEOS and contact associations are finite Float64 checks, not exact-rational or Float32 guarantees. First and subsequent normal CPU calls are listed; no isolation/device claim. Optional Memray records one full construction after imports/resident input preparation, including validation and retained output; decode, bank generation, plotting/serialization, full OS/driver memory and copy traffic are excluded. Requested bytes include realloc requests, not unique/live bytes. Native neighbor propagation, navigation/collision, World, motion/p99 and device memory acceptance remain open.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    display(original, faces, products, water, args.output)
    print(json.dumps(report))


def clipped(triangles, bounds):
    result = []
    for triangle in triangles:
        polygon = triangle.tolist()
        for axis, value, sign in (
            (0, bounds[0], 1),
            (0, bounds[2], -1),
            (1, bounds[1], 1),
            (1, bounds[3], -1),
        ):
            if not polygon:
                break
            points = []
            for previous, current in zip(
                polygon[-1:] + polygon[:-1], polygon, strict=True
            ):
                a, b = np.array(previous), np.array(current)
                da, db = (a[axis] - value) * sign, (b[axis] - value) * sign
                if (da < 0) != (db < 0):
                    points.append((a + da / (da - db) * (b - a)).tolist())
                if db >= 0:
                    points.append(b.tolist())
            polygon = points
        result.extend(
            [polygon[0], polygon[i], polygon[i + 1]] for i in range(1, len(polygon) - 1)
        )
    return result


def display(original, original_faces, products, water, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    low, high = original[:, :2].min(axis=0), original[:, :2].max(axis=0)
    bounds = [*low, *high]
    figure = plt.figure(figsize=(12, 6), layout="constrained")
    for index, (vertices, faces, title) in enumerate(
        (
            (original, original_faces, "Original captured posting surface"),
            (
                products["terrain_vertices"],
                products["terrain_faces"],
                "Conformed terrain and fixed bank body",
            ),
        ),
        1,
    ):
        axis = figure.add_subplot(1, 2, index, projection="3d")
        axis.add_collection3d(
            Poly3DCollection(
                clipped(vertices[faces], bounds),
                facecolors="sandybrown",
                edgecolors="peru",
                linewidth=0.1,
            )
        )
        if index == 2:
            axis.add_collection3d(
                Poly3DCollection(
                    clipped(products["body_vertices"][products["body_faces"]], bounds),
                    facecolors="tan",
                    edgecolors="saddlebrown",
                    linewidth=0.15,
                )
            )
        axis.add_collection3d(
            Poly3DCollection(
                clipped(water["vertices"][water["faces"]], bounds),
                facecolors="steelblue",
                alpha=0.7,
            )
        )
        z0, z1 = original[:, 2].min() - 1, original[:, 2].max() + 1
        axis.set(
            xlim=(low[0], high[0]), ylim=(low[1], high[1]), zlim=(z0, z1), title=title
        )
        axis.set_box_aspect([*(high - low), z1 - z0])
        axis.view_init(elev=23, azim=-125)
    figure.suptitle("Whole supplied window; reported outer obligations remain open")
    figure.savefig(output / "contact.png", dpi=140)
    plt.close(figure)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--face", type=int, required=True)
    parser.add_argument("--bounds", type=float, nargs=4, required=True)
    parser.add_argument("--depth", type=float, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--allocations", action="store_true")
    run(parser.parse_args())
