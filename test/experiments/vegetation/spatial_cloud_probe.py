"""Cold real-host cloud comparisons before a costly complete spatial crown atlas."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from cloud_experiment import view, view_bounds
from foliage_views import bake_views, select_view
from group_cloud import greedy_cloud
from spatial_group_source import load_source


def compare(source, output, queries):
    before = time.perf_counter()
    spatial = greedy_cloud(source, size=16)
    spatial_s = time.perf_counter() - before
    before = time.perf_counter()
    angular = bake_views(source, count=64, size=16)
    angular_s = time.perf_counter() - before
    smaller = angular[::4]
    # Every method uses the same viewport, derived once from all candidate geometry.
    points = np.vstack(
        [source["vertices"], *[c.mesh["vertices"] for c in (*spatial, *angular)]]
    )
    rows = []
    images = []
    for query, direction in enumerate(queries):
        bounds = view_bounds(points, direction)
        for name, charts in (
            ("spatial", spatial),
            ("views16_subset", smaller),
            ("views64", angular),
        ):
            chosen = (
                charts if name == "spatial" else (select_view(charts, direction)[1],)
            )
            metrics, reference, reduced = view(
                source, chosen, direction, bounds, fields=True
            )
            rows.append({"query": query, "method": name, **metrics})
            if query < 3:
                images.append((query, name, reference, reduced))
    np.testing.assert_array_equal(
        np.sort(np.concatenate([c.members for c in spatial])),
        np.arange(len(source["ids"])),
    )
    for c in spatial:
        vertices = source["vertices"][
            source["faces"][np.isin(source["organ_faces"], c.members)]
        ]
        assert np.max(abs(vertices @ c.frame[:, 2] - c.offset)) <= 0.015 + 1e-12
    output.mkdir()
    np.savez_compressed(
        output / "source.npz",
        **{k: v for k, v in source.items() if isinstance(v, np.ndarray)},
    )
    np.savez_compressed(
        output / "coverage.npz",
        **{
            f"{name}-{q}-{kind}": a
            for q, name, ref, red in images
            for kind, a in (("reference", ref), ("reduced", red))
        },
    )
    for name, charts in (("spatial", spatial), ("views64", angular)):
        np.savez_compressed(
            output / (name + ".npz"),
            colour=np.array([c.colour for c in charts]),
            normal=np.array([c.normal for c in charts]),
            frames=np.array([c.frame for c in charts]),
            vertices=np.array([c.mesh["vertices"] for c in charts]),
        )
    return {
        "host_node": source["host_node"],
        "leaves": len(source["ids"]),
        "physical_area_m2": float(source["physical_leaf_area_m2"].sum()),
        "source_triangles": len(source["faces"]),
        "spatial_charts": len(spatial),
        "spatial_triangles": len(spatial) * 2,
        "selected_view_triangles": 2,
        "spatial_build_s": spatial_s,
        "views64_build_s": angular_s,
        "spatial_field_bytes": sum(c.colour.nbytes + c.normal.nbytes for c in spatial),
        "views64_field_bytes": sum(c.colour.nbytes + c.normal.nbytes for c in angular),
        "comparisons": rows,
    }


def study(inputs, output, hosts=4):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private outputs must be under /tmp")
    if type(hosts) is not int or not 1 <= hosts <= 16:
        raise ValueError("one through16 actual host cases required")
    output.mkdir(parents=True, exist_ok=False)
    began = time.perf_counter()
    source = load_source(inputs)
    summary = source.summary()
    assert not source.prototypes and not source.crown._groups
    nodes = source.crown.hosts[
        np.linspace(0, len(source.crown.hosts) - 1, hosts, dtype=int)
    ]
    queries = [
        (np.cos(a), np.sin(a), z)
        for z in (-0.8, -0.05, 0.05, 0.8)
        for a in (0.4, 1.8, 3.2)
    ]
    results = []
    with patch(
        "crown_shoots.CrownShoots.tables",
        side_effect=AssertionError("whole leaf table expansion"),
    ):
        for node in nodes:
            built = time.perf_counter()
            geometry = source.geometry(int(node))
            result = compare(geometry, output / f"host-{node}", queries)
            result["whole_host_case_s"] = time.perf_counter() - built
            results.append(result)
            assert len(source.crown._groups) == 1
            print(
                json.dumps({k: v for k, v in result.items() if k != "comparisons"}),
                flush=True,
            )
    report = {
        "source": summary,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "nodes": list(map(int, nodes)),
        "cases": results,
        "query_directions": queries,
        "wall_s": time.perf_counter() - began,
        "retained_pose_groups": len(source.crown._groups),
        "source_blade_prototypes": len(source.prototypes),
        "source_blade_triangles": sum(
            len(v["faces"]) for v in source.prototypes.values()
        ),
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Actual same-source group/coverage diagnostic, nearest opaque layer and old mean-normal lighting only; no admitted whole-crown/RT/shadow/LOD/native representation.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "cases"}), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for key in ("inputs", "output"):
        p.add_argument("--" + key, type=Path, required=True)
    p.add_argument("--hosts", type=int, default=4)
    a = p.parse_args()
    study(a.inputs, a.output, a.hosts)
