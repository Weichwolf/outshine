"""Fair real-host coverage comparison and separate shared normal-field verification."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from cloud_experiment import view, view_bounds
from foliage_selection import directions
from group_cloud import greedy_cloud
from leaf_patch_catalog import LeafPatchCatalog
from spatial_group_source import load_source
from spatial_normal_fields import encode


def compare(source, catalogs, node, output):
    start = time.perf_counter()
    exact = source.geometry(node)
    exact_s = time.perf_counter() - start
    start = time.perf_counter()
    methods = {"spatial": greedy_cloud(exact, size=16)}
    timings = {
        "spatial_build_s": time.perf_counter() - start,
        "source_group_s": exact_s,
    }
    groups = {}
    for count, catalog in catalogs.items():
        start = time.perf_counter()
        group = catalog.group(node)
        timings[f"patch{count}_build_group_s"] = time.perf_counter() - start
        groups[count] = group
        methods[f"patch{count}"] = catalog.charts(group)
        np.testing.assert_array_equal(group["poses"]["ids"], exact["ids"])
        np.testing.assert_array_equal(
            group["physical_leaf_area_m2"], exact["physical_leaf_area_m2"]
        )
    points = np.vstack(
        [
            exact["vertices"],
            *[c.mesh["vertices"] for charts in methods.values() for c in charts],
        ]
    )
    rows = []
    images = {}
    for query, direction in enumerate(directions(24)):
        bounds = view_bounds(points, direction)
        for name, charts in methods.items():
            for pixels in (4, 8, 16):
                metrics, reference, reduced = view(
                    exact, charts, direction, bounds, pixels, fields=True
                )
                rows.append(
                    {"query": query, "method": name, "pixels": pixels, **metrics}
                )
                if query < 2 and pixels == 8:
                    images[f"{name}-{query}-reference"] = reference
                    images[f"{name}-{query}-reduced"] = reduced
    output.mkdir()
    np.savez_compressed(output / "coverage.npz", **images)
    np.savez_compressed(
        output / "source.npz",
        **{k: a for k, a in exact.items() if isinstance(a, np.ndarray)},
    )
    for count, g in groups.items():
        np.savez_compressed(
            output / f"group-{count}.npz",
            **g["poses"],
            physical_leaf_area_m2=g["physical_leaf_area_m2"],
        )
    return {
        "host_node": node,
        "leaves": len(exact["ids"]),
        "source_triangles": len(exact["faces"]),
        "method_triangles": {k: 2 * len(v) for k, v in methods.items()},
        "timings": timings,
        "instance_array_payload_bytes": sum(
            a.nbytes for a in groups[4]["poses"].values()
        )
        + groups[4]["physical_leaf_area_m2"].nbytes,
        "comparisons": rows,
    }


def normal_checks(source, catalogs, output):
    lights = directions(24)
    rows = []
    for count, catalog in catalogs.items():
        for variant, prototype in catalog.prototypes.items():
            mesh = source.blades[variant].mesh(8, 1)
            for part, chart in enumerate(prototype.charts):
                mask = prototype.face_partition == part
                partial = mesh | {
                    "faces": mesh["faces"][mask],
                    "normals": mesh["normals"].reshape(-1, 3, 3)[mask].reshape(-1, 3),
                }
                measure, errors, mean = encode(
                    partial, chart.frame, chart.mesh["vertices"], lights
                )
                np.testing.assert_array_equal(
                    measure.moments[4], prototype.normal_moments[part]
                )
                error = next(r for r in errors if r["partitions"] == 4)
                rows.append(
                    {
                        "patches": count,
                        "variant": int(variant),
                        "part": part,
                        "mean": mean,
                        "four_bins": error,
                    }
                )
    (output / "normal-report.json").write_text(
        json.dumps(
            {
                "query_lights": lights.tolist(),
                "cases": rows,
                "scope": "Finite same-chart source cosine response; no calibrated RGB/PBR, group R/T, neighbour shadow or arbitrary-angle guarantee.",
            },
            indent=2,
        )
        + "\n"
    )
    for count, catalog in catalogs.items():
        arrays = {}
        for variant, p in catalog.prototypes.items():
            arrays[f"variant{variant}_face_partition"] = p.face_partition
            for part, c in enumerate(p.charts):
                for name, a in c.mesh.items():
                    arrays[f"v{variant}_p{part}_{name}"] = a
                for name, a in (
                    ("colour", c.colour),
                    ("normal", c.normal),
                    ("frame", c.frame),
                    ("moments", p.normal_moments[part]),
                ):
                    arrays[f"v{variant}_p{part}_{name}"] = a
        np.savez_compressed(output / f"prototypes-{count}.npz", **arrays)


def study(inputs, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under /tmp")
    output.mkdir(parents=True, exist_ok=False)
    start = time.perf_counter()
    source = load_source(inputs)
    catalogs = {n: LeafPatchCatalog(load_source(inputs), n) for n in (1, 2, 4)}
    with (
        patch(
            "outshine_ref.vegetation.Shoot.detail",
            side_effect=AssertionError("poses before demand"),
        ),
        patch(
            "outshine_ref.vegetation.Blade.mesh",
            side_effect=AssertionError("mesh before demand"),
        ),
    ):
        for c in catalogs.values():
            assert c.group(int(source.crown.hosts[0]), 0) is None
    summary = source.summary()
    rows = []
    with patch(
        "crown_shoots.CrownShoots.tables",
        side_effect=AssertionError("whole leaf table"),
    ):
        for node in source.crown.hosts[
            np.linspace(0, len(source.crown.hosts) - 1, 4, dtype=int)
        ]:
            rows.append(compare(source, catalogs, int(node), output / f"host-{node}"))
            print(
                json.dumps({k: v for k, v in rows[-1].items() if k != "comparisons"}),
                flush=True,
            )
    before_verify = time.perf_counter()
    normal_checks(source, catalogs, output)
    result = {
        "source": summary,
        "catalogs": {n: c.summary() for n, c in catalogs.items()},
        "cases": rows,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "directions": directions(24).tolist(),
        "host_comparison_wall_s": before_verify - start,
        "separate_normal_verification_s": time.perf_counter() - before_verify,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Same exact original leaf poses, four real hosts, shared eight-variant patches. Enlarged/coarse geometry and qualitative lighting separate; no world/native/LOD/RT/wind/shadow admission.",
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({k: v for k, v in result.items() if k != "cases"}), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for k in ("inputs", "output"):
        p.add_argument("--" + k, type=Path, required=True)
    a = p.parse_args()
    study(a.inputs, a.output)
