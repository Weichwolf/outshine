"""Compare sparse provenance with complete pairs on the preserved planar fixture."""

import argparse
import hashlib
import json
import platform
import resource
import tempfile
import time
import tracemalloc
from pathlib import Path

import numpy as np
import shapely
from river_membership import face_membership
from river_surface import source_polygons, surface_mesh


def plot(xy, faces, polygons, result, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection

    figure, axes = plt.subplots(1, 2, figsize=(12.8, 7.2), layout="constrained")
    for i, polygon in enumerate(polygons):
        colour = plt.cm.tab20(i % 20)
        for ring in (polygon.exterior, *polygon.interiors):
            line = np.asarray(ring.coords)
            axes[0].plot(line[:, 0], line[:, 1], color=colour, linewidth=1)
    collection = PolyCollection(
        xy[faces],
        array=np.diff(result.offsets).astype(float),
        cmap="viridis",
        edgecolors="grey",
        linewidths=0.15,
    )
    axes[1].add_collection(collection)
    figure.colorbar(
        collection, ax=axes[1], label="Positive-area original surface members"
    )
    for axis in axes:
        axis.set(
            xlim=(xy[:, 0].min(), xy[:, 0].max()),
            ylim=(xy[:, 1].min(), xy[:, 1].max()),
            xlabel="Legacy local X [m]",
            ylabel="Legacy local Y [m]",
        )
        axis.set_aspect("equal")
    axes[0].set_title(f"{len(polygons)} original source footprints / holes, unchanged")
    axes[1].set_title("Same conforming faces; sparse provenance")
    figure.suptitle("Planar source-membership diagnostic; no height / WGS84 claim")
    figure.savefig(output, dpi=120)
    plt.close(figure)


def compare(data, spacing_m):
    polygons = source_polygons(data)
    body = shapely.union_all(polygons)
    mesh, coverage = surface_mesh(body, spacing_m)
    variants, results = {}, {}
    for name, indexed in (("indexed", True), ("complete_pairs", False)):
        tracemalloc.start()
        began = time.perf_counter()
        result = face_membership(
            mesh.vertices, mesh.triangles, polygons, indexed=indexed
        )
        elapsed = time.perf_counter() - began
        _, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        variants[name] = dict(
            result.work, instrumented_wall_s=elapsed, python_traced_peak_bytes=peak
        )
        results[name] = result
    for name in (
        "offsets",
        "members",
        "representative",
        "member_area_m2",
        "uncovered_area_m2",
        "status",
    ):
        np.testing.assert_array_equal(
            getattr(results["indexed"], name), getattr(results["complete_pairs"], name)
        )
    indexed = results["indexed"]
    if np.any(indexed.status):
        raise ValueError("fixture provenance has missing/rejected faces")
    counts = np.bincount(indexed.members, minlength=len(polygons))
    missing = indexed.uncovered_area_m2
    if missing.sum() > max(1e-8, body.area * 1e-12):
        raise ValueError("positive provenance does not cover the fixture triangles")
    return (
        mesh,
        polygons,
        indexed,
        {
            "coverage": coverage,
            "variants": variants,
            "complete_pair_arrays_identical": True,
            "positive_sources_per_face_max": int(
                np.diff(indexed.offsets).max(initial=0)
            ),
            "multiple_source_faces": int((np.diff(indexed.offsets) > 1).sum()),
            "uncovered_area_sum_m2": float(missing.sum()),
            "uncovered_area_max_m2": float(missing.max(initial=0)),
            "faces_with_nonzero_uncovered_area": int((missing > 0).sum()),
            "source_reference_counts": counts.tolist(),
            "unused_original_sources": np.flatnonzero(counts == 0).tolist(),
        },
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--spacing-m", type=float, default=20)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private outputs belong in system temp, outside the checkout")
    output.mkdir(parents=True, exist_ok=False)
    manifest = json.loads(args.input.with_suffix(".json").read_text())
    digest = hashlib.sha256(args.input.read_bytes()).hexdigest()
    if digest != manifest["input_sha256"]:
        raise ValueError("fixture digest does not match source manifest")
    with np.load(args.input, allow_pickle=False) as data:
        original_indices = data["surface_source_indices"].copy()
        mesh, polygons, result, report = compare(data, args.spacing_m)
    observation = manifest["source_semantics_observation"]
    np.testing.assert_array_equal(
        original_indices, [r["fixture_index"] for r in observation["surfaces"]]
    )
    report["source_usage"] = [
        {
            "source_table_index": i,
            "original_surface_index": int(original),
            "state": "used" if count else "omitted",
            "face_references": count,
            "reason": None if count else "no_positive_area_in_selected_faces",
        }
        for i, (original, count) in enumerate(
            zip(original_indices, report["source_reference_counts"], strict=True)
        )
    ]
    np.savez(
        output / "membership.npz",
        planar_xy=mesh.vertices[:, :2],
        triangles=mesh.triangles,
        offsets=result.offsets,
        members=result.members,
        representative=result.representative,
        member_area_m2=result.member_area_m2,
        uncovered_area_m2=result.uncovered_area_m2,
        status=result.status,
        original_surface_indices=original_indices,
    )
    (output / "original-source-table.json").write_text(
        json.dumps(observation, indent=2) + "\n"
    )
    plot(
        mesh.vertices[:, :2],
        mesh.triangles,
        polygons,
        result,
        output / "membership.png",
    )
    proc_memory = {}
    if platform.system() == "Linux":
        for line in Path("/proc/self/status").read_text().splitlines():
            if line.startswith(("VmRSS:", "VmHWM:")):
                key, value, _ = line.split()
                proc_memory[key.removesuffix(":")] = int(value) * 1024
    report.update(
        {
            "scope": "Existing planar Ill rings only. No geodetic height, hydrological admission, solver, native format or device gate.",
            "fixture_sha256": digest,
            "manifest_sha256": hashlib.sha256(
                args.input.with_suffix(".json").read_bytes()
            ).hexdigest(),
            "frame": manifest["frame"],
            "spacing_m": args.spacing_m,
            "shapely": shapely.__version__,
            "geos": shapely.geos_version_string,
            "platform": platform.platform(),
            "rusage_maxrss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
            * (1 if platform.system() == "Darwin" else 1024),
            "linux_process_memory_bytes": proc_memory,
            "memory_scope": "Python tracemalloc excludes GEOS workspace; whole-process RSS is cumulative, not per algorithm. RUSAGE and Linux VmHWM disagree on this host; retain both raw values, no per-method/native memory bound. Outputs/index are not a dense face×source matrix. Timings include instrumentation.",
            "artifacts_sha256": {
                p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                for p in sorted(output.iterdir())
            },
        }
    )
    (output / "report.json").write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n"
    )
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
