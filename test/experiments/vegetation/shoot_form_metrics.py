"""Independent rotation/anchor checks, equal inventory and separated form costs."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
import tracemalloc
from pathlib import Path

import numpy as np
from demand import Camera
from scipy.spatial.transform import Rotation
from shoot_form import ShootForm
from shoot_form_probe import TRAITS
from spatial_group_source import load_source


def pose_digest(poses):
    digest = hashlib.sha256()
    for key in ("ids", "variants", "origins", "frames", "nodes"):
        digest.update(poses[key].tobytes())
    return digest.hexdigest()


def scalar_area(mesh):
    p = mesh["vertices"][mesh["faces"]]
    return (
        np.linalg.norm(np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]), axis=1).sum() / 2
    )


def verify_null_original(source, poses):
    for i, node in enumerate(source.crown.hosts):
        source.crown._groups.clear()
        original = source.crown.group(int(node), 100000)
        lo, hi = map(int, source.crown.offsets[i : i + 2])
        for key in poses:
            if poses[key][lo:hi].tobytes() != original[key].tobytes():
                raise AssertionError(f"zero form changed original {key}")
    return len(poses["ids"])


def geometry_checks(source, form, actual, original):
    frames = actual["frames"]
    root_error = (
        actual["origins"] - source.crown.petiole_m * frames[:, :, 1] - actual["nodes"]
    )
    expected = (
        Rotation.from_rotvec(
            original["frames"][:, :, 1] * form.roll[:, None]
        ).as_matrix()
        @ original["frames"]
    )
    rotation_error = float(abs(frames - expected).max())
    np.testing.assert_array_equal(frames[:, :, 1], original["frames"][:, :, 1])
    error = float(np.linalg.norm(root_error, axis=1).max())
    gram = float(abs(frames.swapaxes(1, 2) @ frames - np.eye(3)).max())
    determinant = float(abs(np.linalg.det(frames) - 1).max())
    if max(rotation_error, error, gram, determinant) > 1e-12:
        raise AssertionError("declared Float64 rotation/root tolerance exceeded")
    n = frames[:, :, 2]
    angle = np.degrees(np.arccos(np.clip(abs(n[:, 2]), 0, 1)))
    counts = np.diff(form.base.data["offsets"]).astype(int)
    concentration = (
        np.linalg.norm(
            np.add.reduceat(n, form.base.data["offsets"][:-1].astype(int)), axis=1
        )
        / counts
    )
    return {
        "maximum_independent_rotation_component_error": rotation_error,
        "maximum_petiole_root_error_m": error,
        "maximum_gram_error": gram,
        "maximum_determinant_error": determinant,
        "inclination_axis_degrees_mean_std": [float(angle.mean()), float(angle.std())],
        "inclination_bins_0_15_30_45_60_90": np.histogram(
            angle, (0, 15, 30, 45, 60, 90)
        )[0].tolist(),
        "mean_host_axis_normal_concentration": float(concentration.mean()),
    }


def replay_queries(form, config):
    records = []
    for view in ("whole", "near"):
        c = config[view]
        camera = Camera(
            c["eye"],
            c["target"],
            fov_y=c["fov_y_deg"],
            width=2560,
            height=1440,
            near=0.05,
            far=1000,
        )
        selected = camera.visible(form.lower, form.upper)
        trials = []
        for _ in range(3):
            start = time.perf_counter()
            poses = form.evaluate(selected, max_leaves=100000, anchors=True)
            trials.append(time.perf_counter() - start)
        tracemalloc.start()
        poses = form.evaluate(selected, max_leaves=100000, anchors=True)
        current, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        records.append(
            {
                "view": view,
                "hosts": int(selected.sum()),
                "leaf_rows": len(poses["ids"]),
                "median_evaluate_s": float(np.median(trials)),
                "output_array_bytes": sum(v.nbytes for v in poses.values()),
                "traced_current_bytes": current,
                "traced_peak_bytes": peak,
                "pose_sha256": pose_digest(poses),
            }
        )
    return records


def run(inputs, study, detail, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    source = load_source(inputs)
    config = json.loads((study / "scene.json").read_text())
    forms = {
        k: ShootForm.load(study / f"{k}.npz", expected_source=source.identity)
        for k in TRAITS
    }
    mask = np.ones(len(source.crown.hosts), bool)
    original = forms["original"].evaluate(mask, max_leaves=100000, anchors=True)
    null_rows = verify_null_original(source, original)
    coarse_area = np.array([scalar_area(b.mesh(8, 1)) for b in source.blades])
    fine_area = np.array([scalar_area(b.mesh(32, 4)) for b in source.blades])
    inventory = {}
    with np.load(detail, allow_pickle=False) as z:
        for view in ("whole", "near"):
            areas = np.where(
                z[view],
                fine_area[source.crown.variants],
                coarse_area[source.crown.variants],
            )
            inventory[view] = float(areas.sum())
    records = []
    for name, form in forms.items():
        poses = form.evaluate(mask, max_leaves=100000, anchors=True)
        records.append(
            {
                "mode": name,
                "form_identity": form.identity,
                **geometry_checks(source, form, poses, original),
                "queries": replay_queries(form, config),
            }
        )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_identity": source.identity,
        "original_null_rows_bit_identical": null_rows,
        "same_complete_faceted_leaf_area_m2_all_modes": inventory,
        "cases": records,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Same inventory/mesh per original ID across modes. Axis-normal statistics describe form, not a BSDF normal distribution or botanical measurement. CPU times shared host; trace current/peak exclude cumulative allocations/traffic. No native/device/LOD admission.",
    }
    output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--inputs", type=Path, required=True)
    p.add_argument("--study", type=Path, required=True)
    p.add_argument("--detail", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    run(args.inputs, args.study, args.detail, args.output)
