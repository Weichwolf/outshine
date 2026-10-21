"""Complete observed water partition -> same original Ill zero mesh/physical recipe."""

import argparse
import json
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
import shapely
from compare_linear_physical_river import captured_solve
from compare_physical_river import inputs as physical_inputs
from compare_water_observation import compare as compare_observation
from native_river_inputs import read_v2, surface_rings
from native_water_observation import NO_INPUT, read_v3
from outshine_ref.contracts import Mesh
from outshine_ref.water.geodetic import local_points
from river_surface import surface_mesh
from water_body_domains import assemble
from water_domain_oracle import all_pairs, check_complete


def source_polygons(data, anchor):
    polygons, inputs, sources = [], [], []
    for index, row in enumerate(data["surfaces"]):
        rings = [
            local_points(anchor, r[:, ::-1], 0)[:, :2]
            for r in surface_rings(data, index)
        ]
        polygons.append(shapely.Polygon(rings[0], rings[1:]))
        info = (
            None if int(row["input"]) == NO_INPUT else data["inputs"][int(row["input"])]
        )
        inputs.append(info)
        sources.append(None if info is None else data["sources"][info["source"]])
    return polygons, inputs, sources


def measured(call):
    tracemalloc.start()
    start, cpu = time.perf_counter(), time.process_time()
    result = call()
    cost = {
        "wall_s": time.perf_counter() - start,
        "cpu_s": time.process_time() - cpu,
        "tracemalloc_peak_bytes": tracemalloc.get_traced_memory()[1],
        "allocation_count": None,
    }
    tracemalloc.stop()
    return result, cost


def bind_ill(bodies, observed, manifest, legacy, path):
    root = path.parent
    old = manifest["historical_v2"]
    original = read_v2(root / old["file"], old["sha256"])
    observation = compare_observation(original, observed, manifest, legacy)
    selected = tuple(
        sorted(
            observation["geometry_mapping"]["surfaces"][i]["observed_indices"][0]
            for i in manifest["original_selection"]["selected_surface_indices"]
        )
    )
    body = next(b for b in bodies if b.sources == selected)
    zero, report = surface_mesh(body.geometry)
    prior, anchor, rows, bounds, _arrays, _meta = physical_inputs(path)
    np.testing.assert_array_equal(zero.vertices[:, :2], prior.vertices[:, :2])
    np.testing.assert_array_equal(zero.triangles, prior.triangles)
    automatic = Mesh(np.c_[zero.vertices[:, :2], prior.vertices[:, 2]], zero.triangles)
    primary = np.c_[
        np.arange(len(zero.triangles)),
        np.arange(len(zero.triangles)) + len(zero.triangles),
    ]
    old_fit, old_program, _ = captured_solve(
        prior, anchor, rows, bounds, primary, "linear"
    )
    new_fit, new_program, _ = captured_solve(
        automatic, anchor, rows, bounds, primary, "linear"
    )
    if old_fit.mesh is None or new_fit.mesh is None:
        raise ValueError("original physical control failed")
    np.testing.assert_array_equal(old_fit.mesh.vertices, new_fit.mesh.vertices)
    for key in ("p", "a"):
        if (old_program[key] != new_program[key]).nnz:
            raise ValueError("physical sparse program changed")
    for key in ("q", "b"):
        np.testing.assert_array_equal(old_program[key], new_program[key])
    return (
        {
            "observed_surface_indices": selected,
            "zero_mesh": report,
            "physical_vertices_exact": True,
            "actual_p_a_q_b_exact": True,
            "physical_fit": dict(new_fit.report),
            "scope": "automatic surface domain; existing laboratory courses/priors/rows explicitly reused",
        },
        zero,
        new_fit.mesh,
    )


def pack(bodies):
    offsets = [0]
    source_offsets = [0]
    raw = []
    indices = []
    for body in bodies:
        encoded = shapely.to_wkb(body.geometry, byte_order=1)
        raw.append(encoded)
        offsets.append(offsets[-1] + len(encoded))
        indices.extend(body.sources)
        source_offsets.append(len(indices))
    return {
        "wkb_bytes": np.frombuffer(b"".join(raw), dtype=np.uint8),
        "wkb_offsets": np.array(offsets, dtype="<u4"),
        "source_offsets": np.array(source_offsets, dtype="<u4"),
        "source_indices": np.array(indices, dtype="<u4"),
    }


def plot(bodies, polygons, ill, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import PathPatch
    from matplotlib.path import Path as PlotPath

    fig, axes = plt.subplots(1, 2, figsize=(12.8, 7.2), dpi=100)
    for panel in axes:
        for index, body in enumerate(bodies):
            coordinates = []
            codes = []
            for polygon in shapely.get_parts(body.geometry):
                for ring in (polygon.exterior, *polygon.interiors):
                    points = np.asarray(ring.coords)
                    coordinates.extend(points)
                    codes.extend(
                        [PlotPath.MOVETO]
                        + [PlotPath.LINETO] * (len(points) - 2)
                        + [PlotPath.CLOSEPOLY]
                    )
            panel.add_patch(
                PathPatch(
                    PlotPath(coordinates, codes),
                    facecolor=plt.colormaps["tab20"](index % 20),
                    edgecolor="black",
                    linewidth=0.15,
                )
            )
        panel.autoscale_view()
        panel.set_aspect("equal")
        panel.set_xlabel("East (m)")
    axes[0].set_ylabel("North (m)")
    axes[0].set_title("All candidate domains; procedural ground assumption")
    bounds = shapely.union_all([polygons[i] for i in ill]).bounds
    axes[1].set_xlim(bounds[0] - 100, bounds[2] + 100)
    axes[1].set_ylim(bounds[1] - 100, bounds[3] + 100)
    axes[1].set_title("Automatically recovered Ill domain; original holes retained")
    fig.tight_layout()
    fig.savefig(output / "water-domains.png")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        type=Path,
        default=Path(__file__).parent / "fixtures/feldkirch-ill-native-inputs.json",
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    start = time.perf_counter()
    manifest = json.loads(args.input.read_text())
    root = args.input.parent
    legacy = json.loads((root / manifest["fixture"]).read_text())
    row = manifest["observed_v3"]
    observed = read_v3(root / row["file"], row["sha256"])
    polygons, inputs, sources = source_polygons(
        observed, legacy["frame"]["origin_lon_lat"]
    )
    preparation = time.perf_counter() - start
    (expected, oracle_work), oracle_cost = measured(
        lambda: all_pairs(polygons, inputs, sources, assume_ground=True)
    )
    (bodies, rejected, work), cost = measured(
        lambda: assemble(polygons, inputs, sources, assume_ground=True)
    )
    if rejected:
        raise ValueError(rejected)
    check_complete(polygons, bodies, expected)
    (tree_bodies, tree_rejected, tree_work), tree_cost = measured(
        lambda: assemble(
            polygons, inputs, sources, assume_ground=True, broad_phase="tree"
        )
    )
    if tree_rejected:
        raise ValueError(tree_rejected)
    check_complete(polygons, tree_bodies, expected)
    strict, strict_rejected, strict_work = assemble(
        polygons, inputs, sources, assume_ground=False
    )
    if strict_rejected or any(len(b.sources) != 1 for b in strict):
        raise ValueError("ground policy leaked")
    partial, missing, partial_work = assemble(
        polygons, inputs, sources, assume_ground=True, relation_limit=10
    )
    ill, zero, physical = bind_ill(bodies, observed, manifest, legacy, args.input)
    arrays = pack(bodies)
    arrays.update(
        ill_zero_vertices=zero.vertices,
        ill_triangles=zero.triangles,
        ill_physical_vertices=physical.vertices,
    )
    np.savez_compressed(args.output / "domains.npz", **arrays)
    plot(bodies, polygons, ill["observed_surface_indices"], args.output)
    report = {
        "original_surfaces": len(polygons),
        "body_count": len(bodies),
        "rejected": rejected,
        "body_sources": [b.sources for b in bodies],
        "body_policies": [b.policy for b in bodies],
        "work": work,
        "cost": cost,
        "oracle_work": oracle_work,
        "oracle_cost": oracle_cost,
        "tree_work": tree_work,
        "tree_cost": tree_cost,
        "strict_no_ground_policy": {"body_count": len(strict), "work": strict_work},
        "partial": {
            "body_sources": [b.sources for b in partial],
            "missing": missing,
            "work": partial_work,
        },
        "ill": ill,
        "preparation_s": preparation,
        "retained_array_payload_bytes": sum(a.nbytes for a in arrays.values()),
        "peak_rss_bytes_linux": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "later WATER3 geometry; candidate recipe, explicit ground assumption; not hydraulic/native admission",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "original_surfaces",
                    "body_count",
                    "work",
                    "cost",
                    "oracle_work",
                    "oracle_cost",
                )
            }
        )
    )


if __name__ == "__main__":
    main()
