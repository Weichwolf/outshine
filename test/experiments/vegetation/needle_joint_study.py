"""Actual same-contact direct light; one selected source/BVH, multiple views/lights."""

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
from needle_joint_contacts import (
    camera_contacts,
    direct_response,
    filtered_fields,
    prepare_source,
)
from needle_joint_plot import plot
from needle_response_study import source_digest
from scatter_reference import Float64Nearest


def verify_selected(scene, contacts, direction, bias, visible, cosine):
    selected = np.flatnonzero(cosine > 0)
    selected = (
        selected[np.linspace(0, len(selected) - 1, min(32, len(selected)), dtype=int)]
        if len(selected)
        else selected
    )
    if not len(selected):
        return {"rays": 0, "triangle_tests": 0}
    if scene.reference is None:
        scene.reference = Float64Nearest(scene.index.vertices, scene.index.faces)
    origins = contacts["points"][selected] + bias * direction
    ids, _, tested = scene.reference.query(
        origins,
        np.broadcast_to(direction, origins.shape),
        scene.index.scale * 2 + 4 * bias,
    )
    np.testing.assert_array_equal(visible[selected], ids < 0)
    return {"rays": len(selected), "triangle_tests": tested}


def one_view(source, scene, view, size, output, index):
    contacts, primary = camera_contacts(source, view, size)
    directions = np.r_[view[None], -view[None], np.eye(3), -np.eye(3)]
    arrays = {key: value for key, value in contacts.items() if key != "size"}
    facts = {
        "view": view.tolist(),
        "directions": directions.tolist(),
        "primary": primary,
        "lights": [],
    }
    fields = {native: [] for native in (64, 16, 4, 1)}
    for light, direction in enumerate(directions):
        variants = []
        for bias in (2e-7, 2e-6, 2e-5):
            cosine, visible, work = direct_response(scene, contacts, direction, bias)
            verification = verify_selected(
                scene, contacts, direction, bias, visible, cosine
            )
            fine = filtered_fields(contacts, cosine, visible, 1)[1]
            variants.append(
                (visible, fine, {"work": work, "float64_probe": verification})
            )
            if bias == 2e-6:
                arrays[f"cosine-{light}"] = cosine
                arrays[f"visible-{light}"] = visible
                for native, items in fields.items():
                    items.append(
                        filtered_fields(contacts, cosine, visible, size // native)
                    )
        central = variants[1][1]
        changed = [int(np.count_nonzero(v[0] != variants[1][0])) for v in variants]
        facts["lights"].append(
            {
                "bias_variants": [v[2] for v in variants],
                "visibility_changes_against_2um": changed,
                "bias_mean_abs_response_delta": [
                    float(abs(v[1] - central).mean()) for v in variants
                ],
            }
        )
    for native, items in fields.items():
        values = np.array(items)
        arrays[f"native-{native}"] = values
        error = values[:, 2] - values[:, 1]
        facts[f"native-{native}"] = {
            "independent_max_abs_error": float(abs(error).max()),
            "independent_rmse": float(np.sqrt(np.mean(error**2))),
            "unshadowed_sum": values[:, 0].sum(axis=(1, 2)).tolist(),
            "joint_sum": values[:, 1].sum(axis=(1, 2)).tolist(),
        }
    arrays["directions"] = directions
    np.savez_compressed(output / f"view-{index}.npz", **arrays)
    facts["retained_contact_array_bytes"] = sum(
        v.nbytes for k, v in contacts.items() if isinstance(v, np.ndarray)
    )
    return facts, arrays["native-64"]


def run(path, output, size):
    output = output.resolve()
    if not output.is_relative_to(Path("/tmp")):
        raise ValueError("private outputs must be under /tmp")
    if size not in (128, 256, 512):
        raise ValueError("explicit128/256/512 sampling study required")
    output.mkdir(parents=True, exist_ok=False)
    source, scene = prepare_source(path)
    views = np.array([[1.0, 0, 0.35], [0, 1, 0.35], [1, 1, 0.8]])
    views /= np.linalg.norm(views, axis=1)[:, None]
    report = {
        "model": "same-contact-opaque-unit-diffuse-v1",
        "size": size,
        "bvh_builds": 1,
        "source_sha256": source_digest(path),
        "source": str(path.resolve()),
        "source_owner": json.loads(path.with_name("source.json").read_text()),
        "source_array_bytes": sum(v.nbytes for v in source.values()),
        "bvh_setup": scene.index.setup,
        "views": [],
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=Path(__file__).parent, text=True
        ).strip(),
    }
    report["implementation_sha256"] = {
        name: source_digest(Path(__file__).with_name(name))
        for name in (
            "needle_joint_contacts.py",
            "needle_joint_study.py",
            "needle_joint_plot.py",
            "scatter_scene.py",
            "scatter_reference.py",
            "surface_normal_transfer.py",
        )
    }
    pictures = []
    for index, view in enumerate(views):
        facts, image = one_view(source, scene, view, size, output, index)
        report["views"].append(facts)
        pictures.append(image)
    report["total_ray_work"] = scene.work
    report["float64_known_array_bytes"] = (
        scene.reference.known_array_bytes if scene.reference else 0
    )
    plot(pictures, output / "comparison.png")
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: report[k]
                for k in ["source_sha256", "size", "bvh_builds", "total_ray_work"]
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--size", type=int, default=256)
    args = parser.parse_args()
    run(args.source, args.output, args.size)
