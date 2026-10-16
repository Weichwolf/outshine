"""Validate actual versioned native cumulus Scene/Depth captures against FP64 integration."""

import argparse
import json
import tempfile
from pathlib import Path

import numpy as np
from cloud_field import Layer
from cloud_shape_transfer import transfer
from cloud_sky_reference import cross_cases, read
from cloud_view_reference import physical_air


def judge(metadata, view_samples, light_samples, expected_profile=1):
    info, images, rays, medium, weather = read(metadata)
    if not info["boundary_sky"] or weather[1, 1] not in (0, 1):
        raise ValueError(
            "the frame must explicitly select a supported boundary-sky profile"
        )
    if weather[1, 0] > 0 and weather[1, 1] != expected_profile:
        raise AssertionError(
            "the native frame must contain the requested procedural profile"
        )
    active = np.linalg.norm(rays[:, 3:6], axis=1) > 0
    sky = active & (images[2, :, 3] == 0)
    layer = Layer(
        radius_m=float(weather[0, 0]),
        base_m=float(weather[0, 1]),
        thickness_m=float(weather[0, 2]),
        extinction_per_m=float(weather[0, 3]),
        cover=float(weather[1, 0]),
    )
    kind = "cell-smooth" if weather[1, 1] == 1 else "stratus"
    _, end = physical_air(rays[active], medium)
    query_m = rays[active, 10] * 1000
    query_m[sky[active]] = end[sky[active]]
    expected = transfer(
        rays[active],
        medium,
        layer,
        info["phase_s"],
        kind,
        view_samples=view_samples,
        light_samples=light_samples,
        query_m=query_m,
    )
    actual = np.c_[images[2, active, :3], images[3, active, :3]].astype(float)
    error = np.abs(actual - expected)
    combined = images[0, active, :3] * expected[:, 3:] + expected[:, :3]
    rgb_error = float(np.max(np.abs(images[1, active, :3] - combined)))
    endpoint = float(
        np.max(np.abs(images[3, sky, 3].astype(float) * 1000 - end[sky[active]]))
    )
    np.testing.assert_array_equal(
        images[1, active, 3].view(np.uint32), images[0, active, 3].view(np.uint32)
    )
    if (
        not np.all(np.isfinite(images))
        or np.max(error) > 1e-4
        or rgb_error > 1e-4
        or endpoint > 1
    ):
        raise AssertionError(
            f"{info['name']}: transfer={np.max(error)} RGB={rgb_error} endpoint_m={endpoint}"
        )
    work = images[4, active].astype(float)
    return {
        "case": info["name"],
        "profile": int(weather[1, 1]),
        "resolution": [info["width"], info["height"]],
        "max_radiance_error": float(np.max(error[:, :3])),
        "max_transmittance_error": float(np.max(error[:, 3:])),
        "max_composition_error": rgb_error,
        "max_endpoint_error_m": endpoint,
        "logical_work": {
            name: None if np.any(work[:, i] < 0) else int(np.sum(work[:, i]))
            for i, name in enumerate(
                ("view_density", "cloud_sources", "union_cells", "clear_sources")
            )
        },
    }


def plot(folder, output, before):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    names = (
        "sky-broken",
        "sky-broken-wind",
        "sky-only",
        "sky-overcast",
        "inside",
        "above",
        "orbit",
    )
    paths = [folder / f"actual.{name}.json" for name in names]
    paths = [path for path in paths if path.exists()]
    fig, axes = plt.subplots(
        len(paths),
        2 if before else 1,
        figsize=(13 if before else 8, max(3, 3 * len(paths))),
        squeeze=False,
        constrained_layout=True,
    )
    for row, metadata in zip(axes, paths, strict=True):
        candidates = [metadata]
        if before:
            candidates = [before / metadata.name, metadata]
        for axis, path in zip(row, candidates, strict=True):
            if not path.exists():
                axis.set_axis_off()
                continue
            info, images, _, _, weather = read(path)
            rgb = np.maximum(
                images[1, :, :3].reshape(info["height"], info["width"], 3) * 4, 0
            )
            axis.imshow(rgb / (1 + rgb))
            label = "CumulusV1" if weather[1, 1] == 1 else "Stratus control"
            axis.set_title(
                f"{label}: {info['name']} / {info['width']}×{info['height']}"
            )
            axis.set_axis_off()
    fig.suptitle(
        "Actual native density profiles; same declared scene/light parameters\n"
        "Display4x/(1+4x); independent sampling/cost evidence in receipt; no Place/budget claim"
    )
    fig.savefig(output / "cloud-cumulus-native.pdf")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--before", type=Path)
    parser.add_argument("--view-samples", type=int, default=32)
    parser.add_argument("--light-samples", type=int, default=6)
    parser.add_argument("--profile", type=int, choices=(0, 1), default=1)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private outputs belong in system temp")
    output.mkdir(parents=True, exist_ok=True)
    rows = [
        judge(path, args.view_samples, args.light_samples, args.profile)
        for path in sorted(args.captures.glob("actual.*.json"))
    ]
    if not rows:
        parser.error("actual native captures are required")
    cross = cross_cases(args.captures) if len(rows) in (26, 27) else None
    plot(args.captures, output, args.before)
    (output / "receipt.json").write_text(
        json.dumps(
            {
                "cases": rows,
                "cross_cases": cross,
                "sampling": [args.view_samples, args.light_samples],
                "scope": "Native/FP64 numerical agreement; finite quadrature convergence and full budget remain separate",
            },
            indent=2,
        )
        + "\n"
    )
    print(
        json.dumps(
            {
                "cases": len(rows),
                "max_L": max(r["max_radiance_error"] for r in rows),
                "max_T": max(r["max_transmittance_error"] for r in rows),
            }
        )
    )


if __name__ == "__main__":
    main()
