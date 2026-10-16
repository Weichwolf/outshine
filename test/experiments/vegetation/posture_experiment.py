"""Complete unchanged-source crown with directed posture and rigid organ replay."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from colonization import pipe_radii
from crown_pair_export import export_pair
from crown_posture import pose_rows, posture
from posture_mesh import pose_wood, wood_owners
from spatial_crown_cost import restore
from spatial_crown_experiment import crown_wood
from tree_entry import load


def compare_methods(crown, mobility, direction, curvature):
    rows = []
    reference = None
    for repeat in range(3):
        for method in ("serial", "layers") if repeat % 2 == 0 else ("layers", "serial"):
            start = time.perf_counter()
            result = posture(
                crown.positions,
                crown.parents,
                mobility,
                direction,
                curvature,
                method=method,
            )
            elapsed = time.perf_counter() - start
            if reference is None:
                reference = result
            for a, b in zip(result, reference):
                np.testing.assert_allclose(a, b, atol=3e-14, rtol=0)
            rows.append({"method": method, "repeat": repeat, "seconds": elapsed})
    return rows


def build(inputs, output, curvature):
    start = time.perf_counter()
    output.mkdir(parents=True, exist_ok=False)
    crown = restore(inputs)
    metadata, products = load(inputs / "spatial")
    loaded = time.perf_counter()
    source_wood, source_rows = products["wood.npz"], products["tables.npz"]
    reconstructed = crown_wood(crown)
    for key, array in reconstructed.items():
        np.testing.assert_array_equal(array, source_wood[key])
    owners = wood_owners(crown, source_wood)
    leaf_owners = np.repeat(crown.hosts, crown.counts)
    radii = pipe_radii(crown.parents, 0.14)
    mobility = np.clip(1 - (radii / 0.07) ** 2, 0, 1)
    direction = np.array((0, 0, -1.0))
    binding_ready = time.perf_counter()
    moved, rotations = posture(
        crown.positions, crown.parents, mobility, direction, curvature
    )
    pose_ready = time.perf_counter()
    canonical = {k.removeprefix("leaf_"): a for k, a in source_rows.items()}
    posed = pose_rows(canonical, leaf_owners, crown.positions, moved, rotations)
    posed_rows = {"leaf_" + k: a for k, a in posed.items()}
    posed_wood = pose_wood(source_wood, owners, crown.positions, moved, rotations)
    geometry_ready = time.perf_counter()
    header = {
        "format": "spatial-crown-posture-1",
        "source_identity": crown.identity,
        "curvature_rad_per_m": curvature,
        "direction": direction.tolist(),
        "max_turn_rad": 0.35,
    }
    digest = hashlib.sha256(json.dumps(header, sort_keys=True).encode())
    for a in (crown.positions, crown.parents, mobility):
        digest.update(a.view(np.uint8))
    identity = digest.hexdigest()
    np.savez_compressed(
        output / "posture.npz",
        source=crown.positions,
        parents=crown.parents,
        mobility=mobility,
        positions=moved,
        rotations=rotations,
        leaf_owners=leaf_owners,
        wood_owners=owners,
    )
    payload_sha256 = hashlib.sha256((output / "posture.npz").read_bytes()).hexdigest()
    (output / "posture.json").write_text(
        json.dumps(
            {**header, "identity": identity, "payload_sha256": payload_sha256}, indent=2
        )
        + "\n"
    )
    export_pair(
        inputs,
        output,
        source_wood,
        posed_wood,
        (source_rows, posed_rows),
        identity,
        scope="Matched segmented wood/identical leaves; authored static posture, no botanical or welded-skin admission.",
    )
    exported = time.perf_counter()
    lengths = np.linalg.norm(
        crown.positions[1:] - crown.positions[crown.parents[1:]], axis=1
    )
    new_lengths = np.linalg.norm(moved[1:] - moved[crown.parents[1:]], axis=1)
    checks = {
        "source_identity": crown.identity,
        "leaves": len(crown.variants),
        "nodes": len(crown.parents),
        "source_triangles": metadata["instanced_triangles"],
        "maximum_length_error_m": float(abs(lengths - new_lengths).max()),
        "minimum_leaf_anchor_z_m": float(posed_rows["leaf_origins"][:, 2].min()),
        "pose_array_bytes": moved.nbytes + rotations.nbytes,
        "saved_product_bytes": sum(
            f.stat().st_size for f in output.rglob("*") if f.is_file()
        ),
        "stages_s": {
            "source_decode": loaded - start,
            "binding_validation": binding_ready - loaded,
            "posture": pose_ready - binding_ready,
            "rigid_geometry": geometry_ready - pose_ready,
            "export": exported - geometry_ready,
        },
        "methods": compare_methods(crown, mobility, direction, curvature),
    }
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "identity": identity,
        "curvature_rad_per_m": curvature,
        "checks": checks,
        "presentation_labels": {
            "recursive": "saved spatial crown",
            "spatial": "directed branch posture",
        },
        "scope": "Same saved plan/IDs/leaves, rigid organs; static generic segmented form experiment, not accepted botanical species, wind, full forest/LOD/native device quality.",
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"identity": identity, "checks": checks}), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--curvature", type=float, default=0.25)
    args = parser.parse_args()
    build(args.inputs, args.output, args.curvature)
