"""Replay native-spacing source CSR and representative selection on frozen originals."""

import argparse
import hashlib
import json
import resource
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np
import shapely
from inspect_source_membership import DEFAULT, inspect
from water_source_binding import NO_SOURCE, bind_sources


def run(method, *, trace=False):
    if trace:
        tracemalloc.start()
    started = time.perf_counter()
    provenance = inspect(DEFAULT)
    with np.load(DEFAULT) as data:
        sources = shapely.get_parts(shapely.from_wkb(data["sources_wkb"].tobytes()))
        union = shapely.from_wkb(data["union_wkb"].tobytes())
        control = shapely.from_wkb(data["triangle_wkb"].tobytes())
        source_ids = data["source_indices"].copy()
    # Existing native default, not a place-specific refinement or new tolerance.
    spacing_m = 20
    triangles = shapely.get_parts(
        shapely.constrained_delaunay_triangles(shapely.segmentize(union, spacing_m))
    )
    raw = np.array([np.array(t.exterior.coords)[:3] for t in triangles])
    vertices, inverse = np.unique(raw.reshape(-1, 2), axis=0, return_inverse=True)
    faces = inverse.astype(np.uint32).reshape(-1, 3)
    prepared = time.perf_counter()
    binding = bind_sources(vertices, faces, sources, source_ids, method=method)
    bound = time.perf_counter()
    tree = shapely.STRtree(sources)
    numeric_empty = owner_changes = missed_sets = 0
    numeric_owners = np.full(len(faces), NO_SOURCE, dtype=np.uint32)
    for i, triangle in enumerate(triangles):
        candidates = sorted(tree.query(triangle), key=lambda j: int(source_ids[j]))
        areas = shapely.area(shapely.intersection(sources[candidates], triangle))
        pairs = [
            (int(source_ids[j]), float(a))
            for j, a in zip(candidates, areas, strict=True)
            if a > 0
        ]
        first, last = binding.offsets[i : i + 2]
        exact_members = binding.sources[first:last].tolist()
        missed_sets += [j for j, _a in pairs] != exact_members
        if pairs:
            numeric_owners[i] = max(pairs, key=lambda p: p[1])[0]
            owner_changes += max(pairs, key=lambda p: p[1])[0] != int(
                binding.representatives[i]
            )
        else:
            numeric_empty += 1
    finished = time.perf_counter()
    memory = {}
    if trace:
        _live, memory["python_traced_peak_bytes"] = tracemalloc.get_traced_memory()
        tracemalloc.stop()
    report = {
        "method": method,
        "binding": binding.report,
        "states": list(binding.states),
        "sources": len(sources),
        "faces": len(faces),
        "vertices": len(vertices),
        "boundary_spacing_m": spacing_m,
        "exact_native_control_matches": sum(
            t.equals_exact(control, 0) for t in triangles
        ),
        "input_sha256": hashlib.sha256(DEFAULT.read_bytes()).hexdigest(),
        "regenerated_wkb_sha256": hashlib.sha256(
            shapely.to_wkb(shapely.GeometryCollection(triangles))
        ).hexdigest(),
        "numeric_no_owner_faces": numeric_empty,
        "numeric_membership_disagreements": missed_sets,
        "numeric_owner_disagreements_on_nonempty_faces": owner_changes,
        "provenance": provenance,
        "phases_s": {
            "input_and_geometry": prepared - started,
            "binding": bound - prepared,
            "numeric_diagnosis": finished - bound,
            "total": finished - started,
        },
        "geometry_array_bytes": vertices.nbytes + faces.nbytes,
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        "scope": "Native-default 20m densification of captured union, with exact original control face retained; no complete native mesh byte replay. Exact maximum original-area owner with smallest original-ID tie, which can correct legacy numeric owner choices. Binding validity is not coverage/physical/native asset admission. No original geometry edit or native code change.",
        **memory,
    }
    arrays = {
        "vertices": vertices,
        "faces": faces,
        "offsets": binding.offsets,
        "sources": binding.sources,
        "representatives": binding.representatives,
        "original_source_ids": source_ids,
        "numeric_representatives": numeric_owners,
    }
    if binding.report["valid"]:
        if (
            np.any(binding.representatives == NO_SOURCE)
            or len(binding.sources) != binding.offsets[-1]
        ):
            raise ValueError("inconsistent complete metadata")
        for i, owner in enumerate(binding.representatives):
            if (
                owner
                not in binding.sources[binding.offsets[i] : binding.offsets[i + 1]]
            ):
                raise ValueError(
                    "representative is not one of the actual source owners"
                )
    return arrays, report


def display(arrays, path):
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection

    triangles = arrays["vertices"][arrays["faces"]]
    ids = np.sort(arrays["original_source_ids"])
    exact, numeric = arrays["representatives"], arrays["numeric_representatives"]
    colors = plt.colormaps["turbo"](np.linspace(0.05, 0.95, len(ids)))
    figure, axes = plt.subplots(1, 2, figsize=(12, 7), sharex=True, sharey=True)
    changed = exact != numeric
    centers = triangles[changed].mean(axis=1)
    for axis, owners, title in (
        (axes[0], numeric, "Numeric per-face diagnosis / group not admitted"),
        (axes[1], exact, "Exact original-area representatives"),
    ):
        rgba = np.tile([0.8, 0.1, 0.1, 1.0], (len(owners), 1))
        valid = owners != NO_SOURCE
        rgba[valid] = colors[np.searchsorted(ids, owners[valid])]
        axis.add_collection(PolyCollection(triangles, facecolors=rgba, linewidths=0))
        axis.scatter(
            centers[:, 0],
            centers[:, 1],
            s=15,
            facecolors="none",
            edgecolors="crimson",
            label="Changed representative",
        )
        axis.autoscale()
        axis.set(
            aspect="equal",
            xlabel="Native East / m",
            ylabel="Native North / m",
            title=title,
        )
        axis.legend(fontsize=7)
    figure.suptitle(
        "Same 20m-spacing geometry; metadata diagnosis, not world or physical-model admission"
    )
    figure.tight_layout()
    figure.savefig(path, dpi=150)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--method", choices=("areas", "relations"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--image", action="store_true")
    args = parser.parse_args()
    arrays, report = run(args.method, trace=args.trace)
    args.output.mkdir(parents=True, exist_ok=True)
    np.savez(args.output / "binding.npz", **arrays)
    if args.image:
        display(arrays, args.output / "binding.png")
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "method": args.method,
                "valid": report["binding"]["valid"],
                "phases_s": report["phases_s"],
                "area_calls": report["binding"]["area_evaluations"],
            }
        )
    )


if __name__ == "__main__":
    main()
