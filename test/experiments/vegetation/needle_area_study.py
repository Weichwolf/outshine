"""Compare clipped source-area quadrature with the unchanged finite screen study."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from needle_area_light import integrate
from needle_joint_contacts import prepare_source
from needle_response_study import source_digest


def run(source_path, screen, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    source, scene = prepare_source(source_path)
    screen_report = json.loads((screen / "report.json").read_text())
    if screen_report["source_sha256"] != source_digest(source_path):
        raise ValueError("same frozen source required")
    view = np.array(screen_report["views"][0]["view"])
    lights = np.array(screen_report["views"][0]["directions"])[[0, 3]]
    with np.load(screen / "view-0.npz") as stored:
        reference = stored["native-16"][[0, 3], 1]
    report = {
        "numpy_version": np.__version__,
        "shapely_version": shapely.__version__,
        "geos_version": shapely.geos_version_string,
        "source_sha256": source_digest(source_path),
        "screen": str(screen.resolve()),
        "view": view.tolist(),
        "lights": lights.tolist(),
        "size": 16,
        "bvh_builds": 1,
        "source_owner": screen_report["source_owner"],
        "bvh_setup": scene.index.setup,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=Path(__file__).parent, text=True
        ).strip(),
        "orders": [],
    }
    report["implementation_sha256"] = {
        name: source_digest(Path(__file__).with_name(name))
        for name in (
            "needle_area_cells.py",
            "needle_area_light.py",
            "needle_area_study.py",
            "needle_joint_contacts.py",
            "surface_normal_transfer.py",
        )
    }
    arrays = {"screen": reference}
    previous = None
    for order in (1, 2, 4, 6):
        before = dict(scene.work)
        began = time.perf_counter()
        fields, work = integrate(source, scene, view, lights, size=16, order=order)
        wall = time.perf_counter() - began
        ray_work = {key: scene.work[key] - before[key] for key in before}
        radiance = fields[[2, 4]]
        diff = radiance - reference
        facts = {
            "order": order,
            "wall_s": wall,
            "work": work,
            "rays": ray_work,
            "screen_max_abs_delta": float(abs(diff).max()),
            "screen_rmse": float(np.sqrt(np.mean(diff**2))),
            "coverage_range": [float(fields[0].min()), float(fields[0].max())],
            "mean_coverage": float(fields[0].mean()),
            "response_mean": radiance.mean((1, 2)).tolist(),
        }
        if previous is not None:
            delta = radiance - previous
            facts.update(
                previous_max_abs_delta=float(abs(delta).max()),
                previous_rmse=float(np.sqrt(np.mean(delta**2))),
            )
        previous = radiance
        report["orders"].append(facts)
        arrays[f"order-{order}"] = fields
    np.savez_compressed(output / "fields.npz", **arrays)
    fig, axes = plt.subplots(2, 5, figsize=(16, 9), dpi=160)
    for row in range(2):
        for col, (label, values) in enumerate(
            [
                ("screen512", reference[row]),
                *[
                    (f"area order{order}", arrays[f"order-{order}"][[2, 4][row]])
                    for order in (1, 2, 4, 6)
                ],
            ]
        ):
            axes[row, col].imshow(
                values, vmin=0, vmax=1, cmap="gray", interpolation="nearest"
            )
            axes[row, col].set_title(f"{label} / light{row} / native16×16")
            axes[row, col].axis("off")
    fig.suptitle(
        "Same source/visibility model; source-area vs screen integration; no fit"
    )
    fig.tight_layout()
    fig.savefig(output / "comparison.png")
    plt.close(fig)
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["orders"]))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--screen", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.source, args.screen, args.output)
