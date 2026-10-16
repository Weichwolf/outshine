"""Separate density work and finite quadrature error from the morphology comparison."""

import argparse
import json
import tempfile
from dataclasses import asdict
from pathlib import Path

import numpy as np
from cloud_shape_compare import load_capture
from cloud_shape_field import KINDS, Work, extinction
from cloud_shape_transfer import transfer
from cloud_view_reference import coefficients, physical_air


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--kind", choices=KINDS)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private outputs belong in system temp")
    output.mkdir(parents=True, exist_ok=True)
    info, all_rays, _, medium, layer = load_capture(args.capture, 16)
    rays = all_rays[::4]
    points = np.random.default_rng(3393).uniform(
        [-10000, 0.01, -10000], [10000, 0.99, 10000], (16384, 3)
    )
    points[:, 1] = layer.base_m + layer.thickness_m * points[:, 1]
    begin, end = physical_air(rays, medium)
    sigma, _ = coefficients(rays, medium)
    clear_t = np.exp(-sigma[0] * (end - begin))
    rows = []
    for kind in [args.kind] if args.kind else KINDS:
        work = Work()
        values = extinction(points, layer, info["phase_s"], kind, work)
        records = []
        refined = None
        for view, light in ((16, 6), (32, 12), (64, 24), (128, 48)):
            value = transfer(
                rays,
                medium,
                layer,
                info["phase_s"],
                kind,
                view_samples=view,
                light_samples=light,
            )
            np.save(output / f"{kind}-{view}-{light}.npy", value)
            if refined is not None:
                error = np.abs(value - refined)
                records.append(
                    {
                        "current": [view, light],
                        "previous": [view // 2, light // 2],
                        "max_radiance_delta": float(np.max(error[:, :3])),
                        "max_transmittance_delta": float(np.max(error[:, 3:])),
                    }
                )
            refined = value
        comparisons = []
        for view, light in ((16, 6), (16, 12), (16, 24), (32, 6), (32, 12), (64, 24)):
            path = output / f"{kind}-{view}-{light}.npy"
            if path.exists():
                value = np.load(path)
            else:
                value = transfer(
                    rays,
                    medium,
                    layer,
                    info["phase_s"],
                    kind,
                    view_samples=view,
                    light_samples=light,
                )
                np.save(path, value)
            error = np.abs(value - refined)
            comparisons.append(
                {
                    "samples": [view, light],
                    "max_radiance_error_to_128_48": float(np.max(error[:, :3])),
                    "max_transmittance_error_to_128_48": float(np.max(error[:, 3:])),
                    "p99_radiance_error": float(np.quantile(error[:, :3], 0.99)),
                }
            )
        opacity = 1 - np.clip(refined[:, 3] / clear_t, 0, 1)
        row = {
            "kind": kind,
            "identical_density_points": len(points),
            "density_work": asdict(work),
            "mean_density_per_m": float(np.mean(values)),
            "density_support_fraction": float(np.mean(values > 0)),
            "view_opacity_mean": float(np.mean(opacity)),
            "view_opacity_above_0_5": float(np.mean(opacity > 0.5)),
            "finite_refinements": records,
            "comparison": comparisons,
        }
        rows.append(row)
        print(json.dumps(row), flush=True)
    (output / "receipt.json").write_text(
        json.dumps(
            {
                "rays": len(rays),
                "cases": rows,
                "scope": "Finite independent FP64 refinements, not a global quadrature bound or native timing",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
