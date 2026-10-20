"""Check ready contour plans in the actual UV0-9/material/closed-mesh consumer."""

import argparse
import hashlib
import json
import platform
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from compare_roof_partition import block_skeletons
from outshine_ref.buildings.modelling import SkeletonLimits, hip_footprints, prepare_hip
from outshine_ref.buildings.modelling.faces import ROOF_TAGS
from outshine_ref.buildings.modelling.skeleton_plan import finish, input_data
from verify_modelling_roof_hip import measure, uv_surface
from verify_modelling_roof_shed import source


def verify_prepared(original, plan, value, mode):
    began = time.perf_counter()
    with (
        block_skeletons("contour"),
        patch(
            "outshine_ref.buildings.modelling.roof_hip.prepare_hip",
            side_effect=AssertionError("Prepared hit reconstructed its roof plan"),
        ),
    ):
        roof = original.roof_hip(value, mode, prepared_plans=(plan,))
    shape_seconds = time.perf_counter() - began
    assert roof.roof_info.plans[0] is plan
    product = roof.emit()
    volume, quality = measure(product)
    integral = sum(
        f.polygon.area
        * (np.array(f.polygon.centroid.coords[0]) @ f.gradient + f.intercept_m)
        for f in plan.facets
    )
    expected = integral * plan.factor(value, mode)
    np.testing.assert_allclose(volume, expected, atol=1e-7, rtol=1e-9)
    checked = uv_surface(original, roof)
    assert roof.geometry.material_defs == original.geometry.material_defs
    assert roof.geometry.group_names == original.geometry.group_names
    assert roof.material == original.material and roof.textures == original.textures
    assert roof.projections == original.projections
    for index, face in enumerate(roof.geometry.polygons):
        owner = original.geometry.polygons[roof.roof_info.source_faces[index]]
        assert face.material == owner.material and face.group == owner.group
        assert owner.tags - ROOF_TAGS.keys() <= face.tags
        assert len(face.tags & ROOF_TAGS.keys()) == 1
    return roof, {
        "mode": mode,
        "shape_seconds": shape_seconds,
        "check_seconds": time.perf_counter() - began - shape_seconds,
        "triangles": len(product["faces"]),
        "uv_values_checked": checked,
        "volume_m3": volume,
        "expected_volume_m3": expected,
        **quality,
    }


def coplanar_control():
    original = source(shapely.Polygon([(0, 0), (10, 0), (10, 6), (5, 6), (0, 6)]))
    domain = hip_footprints(original)[0]
    with block_skeletons("contour"):
        plan = prepare_hip(domain, "contour")
    nodes = plan.positions_m.copy()
    middle = np.flatnonzero(np.linalg.norm(nodes - [5, 3], axis=1) < 1e-9)
    assert len(middle) == 1
    nodes[middle[0], 0] = 6.5
    origin, local, _, boundaries = input_data(domain, SkeletonLimits())
    alternate = finish(
        origin,
        local,
        boundaries,
        (nodes, plan.distances_m.copy(), tuple(f.node_indices for f in plan.facets), 0),
        "analytic-coplanar-control",
        SkeletonLimits(),
        time.perf_counter(),
    )
    differences = [
        a.polygon.symmetric_difference(b.polygon).area
        for a, b in zip(plan.facets, alternate.facets, strict=True)
    ]
    np.testing.assert_allclose(sorted(differences), [0, 0, 0, 2.25, 2.25], atol=1e-10)
    return original, plan, alternate


def write_uv_inspection(output):
    from modelling_scope_probe import engine_mesh
    from showcase_uv import texture

    original, first, second = coplanar_control()
    vertices, faces, uv, offset = [], [], [], 0
    for index, plan in enumerate((first, second)):
        roof, _ = verify_prepared(original, plan, 3.0, "byHeight")
        mesh = engine_mesh(roof.emit())
        points = mesh["vertices"].copy()
        points[:, 0] += index * 12 - 6
        vertices.append(points)
        faces.append(mesh["faces"] + offset)
        uv.append(mesh["uv"])
        offset += len(points)
    path, atlas = output / "coplanar-uv.npz", output / "control-texture.png"
    np.savez_compressed(
        path,
        vertices=np.concatenate(vertices),
        faces=np.concatenate(faces),
        uv=np.concatenate(uv),
    )
    texture(atlas)
    config = {
        "mesh": str(path.resolve()),
        "texture": str(atlas.resolve()),
        "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "texture_sha256": hashlib.sha256(atlas.read_bytes()).hexdigest(),
        "uv_channel": "uv",
        "profile": [1920, 1080],
        "output": str((output / "coplanar-uv.png").resolve()),
        "eye": [5, -20, 25],
        "target": [5, 3, 1],
        "ortho_scale": 28.0,
        "labels": [
            {"text": "original cut", "position": [-6, -0.8, 0]},
            {"text": "cut shifted 1.5 m", "position": [6, -0.8, 0]},
        ],
    }
    (output / "scene.json").write_text(json.dumps(config, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--uv-inspection",
        action="store_true",
        help="Write inputs for existing render_uv_inspection.py; does not run Blender",
    )
    args = parser.parse_args()
    path = Path(__file__).with_name("reference") / "hip-plans.json"
    fixture = json.loads(path.read_text())
    results = []
    for row in fixture["records"]:
        result = {"id": row["id"]}
        stage = "source"
        try:
            original = source(shapely.Polygon(row["outer"], row["holes"]))
            stage = "prepare"
            began = time.perf_counter()
            with block_skeletons("contour"):
                plan = prepare_hip(hip_footprints(original)[0], "contour")
            result["prepare_seconds"] = time.perf_counter() - began
            result["modes"] = []
            for value, mode in ((3.0, "byHeight"), (30.0, "byAngle")):
                stage = "consume:" + mode
                _, stats = verify_prepared(original, plan, value, mode)
                result["modes"].append(stats)
            result["accepted"] = True
        except (ValueError, RuntimeError, ArithmeticError, AssertionError) as error:
            result.update(
                accepted=False,
                stage=stage,
                error=type(error).__name__ + ": " + str(error),
            )
        results.append(result)
    sources = [Path(__file__), Path(__file__).with_name("test_roof_partition.py")]
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "working_tree_dirty": bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True)
        ),
        "platform": platform.platform(),
        "fixture_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "probe_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sources
        },
        "scope": "Original48 intrinsic plans, byHeight/byAngle, UV0-9, source appearance, closed oriented volume; no PBR/device/peak proof",
        "rows": results,
    }
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "consumer.json").write_text(json.dumps(report, indent=2) + "\n")
    accepted = sum(r["accepted"] for r in results)
    print(
        f"Prepared contour consumer: {accepted}/{len(results)} sources, two height modes"
    )
    if accepted != len(results):
        raise SystemExit(1)
    if args.uv_inspection:
        write_uv_inspection(args.output)


if __name__ == "__main__":
    main()
