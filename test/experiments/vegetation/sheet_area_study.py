"""Actual two-sided leaves: material/normal/visibility correlation before filtering."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from normal_lobe_study import coarsen
from scatter_scene import ScatterScene
from sheet_leaf_source import build_source
from sheet_light_measure import SheetMeasure
from surface_area_light import integrate


def footprint_comparison(measure, index, arrays):
    levels = []
    coverage, optical, cosines = (
        measure.coverage[:, None],
        measure.optical_sum,
        measure.cosines.transpose(1, 0, 2),
    )
    joint, unshadowed = (
        measure.joint.transpose(1, 0, 2),
        measure.unshadowed.transpose(1, 0, 2),
    )
    for size in (64, 32, 16, 8, 4, 2, 1):
        mean = np.divide(
            optical, coverage, out=np.zeros_like(optical), where=coverage > 0
        )
        separate = (
            cosines[..., :1] * mean[:, None, :3] + cosines[..., 1:] * mean[:, None, 3:]
        ) / np.pi
        difference = separate - unshadowed
        shadow = unshadowed - joint
        levels.append(
            {
                "size": size,
                "separate_unshadowed_max": float(abs(difference).max()),
                "separate_unshadowed_rmse": float(np.sqrt(np.mean(difference**2))),
                "source_shadow_max": float(shadow.max()),
                "source_shadow_rmse": float(np.sqrt(np.mean(shadow**2))),
            }
        )
        arrays[f"separate{index}_{size}"] = separate
        if size > 1:
            coverage, optical, cosines, joint, unshadowed = (
                coarsen(a, size)
                for a in (coverage, optical, cosines, joint, unshadowed)
            )
    return levels


def run(output, order):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private outputs must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    source, identity = build_source()
    np.savez_compressed(
        output / "source.npz",
        **{k: v for k, v in source.items() if isinstance(v, np.ndarray)},
    )
    (output / "source.json").write_text(json.dumps(identity, indent=2) + "\n")
    scene = ScatterScene(source["vertices"], source["faces"])
    views = np.array([[0.2, -0.6, 0.77], [-0.2, 0.6, -0.77]])
    views /= np.linalg.norm(views, axis=1)[:, None]
    lights = np.array([[0.2, -0.3, 1.0], [-0.2, 0.3, -1.0], [1, 0.2, 0.15]])
    lights /= np.linalg.norm(lights, axis=1)[:, None]
    arrays, records = {"views": views, "lights": lights}, []
    for index, view in enumerate(views):
        measure = SheetMeasure(source, scene, view, lights, size=64)
        before, began = dict(scene.work), time.perf_counter()
        _, work = integrate(
            source,
            scene,
            view,
            [view],
            size=64,
            order=order,
            max_samples=measure.max_points,
            contact_measure=measure,
            two_sided=True,
        )
        if (
            np.min(measure.joint) < 0
            or np.max(measure.joint - measure.unshadowed) > 2e-12
            or np.max(measure.unshadowed - measure.coverage[None, :, None] / np.pi)
            > 2e-12
        ):
            raise AssertionError("finite thin-sheet energy/order bound violated")
        arrays[f"coverage{index}"] = measure.coverage
        for name in ("joint", "unshadowed", "reflection", "optical_sum", "cosines"):
            arrays[f"{name}{index}"] = getattr(measure, name)
        levels = footprint_comparison(measure, index, arrays)
        records.append(
            {
                "camera": index,
                "seconds": time.perf_counter() - began,
                "source_work": work,
                "measure_work": measure.work,
                "rays": {k: scene.work[k] - before[k] for k in before},
                "collector_bytes": sum(
                    a.nbytes
                    for a in (
                        measure.coverage,
                        measure.optical_sum,
                        measure.cosines,
                        measure.joint,
                        measure.unshadowed,
                        measure.reflection,
                    )
                ),
                "levels": levels,
            }
        )
        print(
            json.dumps(
                {
                    "camera": index,
                    "seconds": records[-1]["seconds"],
                    "rays": records[-1]["rays"]["rays"],
                }
            ),
            flush=True,
        )
    np.savez_compressed(output / "fields.npz", **arrays)
    (output / "report.json").write_text(
        json.dumps(
            {
                "schema": "sheet-joint-area-pilot-1",
                "order": order,
                "source": identity,
                "views": records,
                "commit": subprocess.check_output(
                    ["git", "rev-parse", "HEAD"], text=True
                ).strip(),
                "scope": "six original thin-sheet leaves, prepared heterogeneous optics, local diffuse R/T with opaque geometric visibility; no ballistic-through-neighbour, BSSRDF, multiple scattering or closed-needle admission",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--order", type=int, choices=(2, 4), default=2)
    args = parser.parse_args()
    run(args.output, args.order)
