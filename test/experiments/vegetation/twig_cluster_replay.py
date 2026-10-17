"""Finished product hit, exact source retention and reverse-demand cluster replay."""

import argparse
import json
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import Shoot
from spatial_crown_replay import check_mesh
from tree_entry import load
from twig_cluster_experiment import read_plan
from twig_clusters import TwigClusters


def geometry(before, after, plan):
    a, b = before["wood.npz"], after["wood.npz"]
    for key in ("vertices", "faces", "normals", "corner_uv", "surface"):
        np.testing.assert_array_equal(a[key], b[key][: len(a[key])])
    report = check_mesh(b)
    assert len(b["faces"]) - len(a["faces"]) == plan.summary()["twigs"] * 32
    area = 0.0
    for v in range(8):
        name = f"blade-{v}.npz"
        for key, array in before[name].items():
            np.testing.assert_array_equal(array, after[name][key])
        m = after[name]
        p = m["vertices"][m["faces"]]
        unit = (
            np.linalg.norm(np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]), axis=1).sum()
            / 2
        )
        area += unit * int(np.count_nonzero(after["tables.npz"]["leaf_variants"] == v))
    for key in ("leaf_ids", "leaf_variants"):
        np.testing.assert_array_equal(
            before["tables.npz"][key], after["tables.npz"][key]
        )
    report["physical_leaf_area_m2"] = float(area)
    return report


def run(inputs, output):
    start = time.perf_counter()
    with (
        patch("colonization.grow", side_effect=AssertionError("growth on hit")),
        patch.object(
            TwigClusters, "group", side_effect=AssertionError("groups on hit")
        ),
        patch.object(Shoot, "detail", side_effect=AssertionError("organs on hit")),
        patch("twig_cluster_mesh.add_twigs", side_effect=AssertionError("wood on hit")),
    ):
        plan = read_plan(inputs)
        before_meta, before = load(inputs / "recursive")
        after_meta, after = load(inputs / "spatial")
        assert after_meta["plan_identity"] == plan.identity
        assert before_meta["plan_identity"] == plan.source_identity
    loaded = time.perf_counter()
    meshes = geometry(before, after, plan)
    assert (
        after_meta["instanced_triangles"]
        == before_meta["instanced_triangles"] + plan.summary()["twigs"] * 32
    )
    assert not plan._prototypes and not plan._groups
    for node in plan.host_ids[::-1]:
        group = plan.group(int(node), 100000)
        ids = group["ids"].astype(int)
        for key in ("ids", "variants", "origins", "frames"):
            np.testing.assert_array_equal(
                group[key], after["tables.npz"]["leaf_" + key][ids]
            )
        np.testing.assert_allclose(
            group["origins"] - plan.petiole * group["frames"][:, :, 1],
            group["nodes"],
            atol=3e-14,
            rtol=0,
        )
    all_rows = plan.tables(100000)
    np.testing.assert_allclose(np.linalg.det(all_rows["frames"]), 1, atol=3e-14, rtol=0)
    result = {
        "identity": plan.identity,
        "summary": plan.summary(),
        "cache_read_s": loaded - start,
        "checks_s": time.perf_counter() - loaded,
        "geometry": meshes,
        "scope": "Source wood exact and physical leaves unchanged; finished hit forbids growth/placement/wood construction, separate reverse-demand replay. Finite segmented geometry, no species/collision/wind/forest/LOD/native/device admission.",
    }
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.output)
