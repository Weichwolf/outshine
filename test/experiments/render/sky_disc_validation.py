"""Check native solar-disc mapping separately from the existing sampled-LUT error."""

import json

import numpy as np
from sky_disc_reference import BACKGROUND, reference

WIDTH, HEIGHT = 1280, 720
SUN_EDGE_ROUNDOFF = 4 * np.finfo("f4").eps
TRANSFER_LIMIT = 1e-3


def image(path):
    return np.fromfile(path, "<f4").reshape(HEIGHT, WIDTH, 4)


def analyze(output):
    rows = []
    for storage in ("f16", "f32"):
        before, after = output / f"before-{storage}", output / f"after-{storage}"
        if (before / "veil.rgba16").read_bytes() != (
            after / "veil.rgba16"
        ).read_bytes():
            raise RuntimeError(
                "before and after must consume the same actual production LUT"
            )
        lut = (
            np.fromfile(after / "veil.rgba16", "<f2")
            .reshape(64, 256, 4)[..., :3]
            .astype(float)
        )
        for file in sorted(after.glob("*.parameters")):
            if file.read_bytes() != (before / file.name).read_bytes():
                raise RuntimeError("before/after must use identical physical inputs")
            parameters = np.fromfile(file, "<f4")
            expected, edge, _ = reference(
                parameters, WIDTH, HEIGHT, lut, endpoints=False
            )
            interior, exterior = edge > SUN_EDGE_ROUNDOFF, edge < -SUN_EDGE_ROUNDOFF
            solid = np.pi * float(parameters[26]) ** 2 / float(parameters[22])
            current = image(file.with_suffix(".rgba32"))
            previous = image(before / file.with_suffix(".rgba32").name)
            for pixels in (previous, current):
                if (
                    not np.isfinite(pixels).all()
                    or np.any(pixels < 0)
                    or np.any(pixels[..., 3] != 1)
                ):
                    raise RuntimeError(
                        "native sky must have finite nonnegative RGB and alpha1"
                    )
                if not np.all(pixels[exterior, :3] == BACKGROUND):
                    raise RuntimeError(
                        "the unchanged nonsolar SkyView radiance must survive exactly"
                    )
            velocity = image(file.with_suffix(".velocity32"))[..., :2]
            if not np.all(velocity == -1e4) or not np.array_equal(
                velocity, image(before / file.with_suffix(".velocity32").name)[..., :2]
            ):
                raise RuntimeError(
                    "the production sky static-velocity contract changed"
                )
            current_error = abs(current[..., :3] - expected)
            old_error = abs(previous[..., :3] - expected)
            peak_t = float(np.max(current_error[interior]) * solid)
            if peak_t > TRANSFER_LIMIT:
                raise RuntimeError(
                    "solar transfer lookup exceeds the incident-radiance budget"
                )
            unchanged = current.tobytes() == previous.tobytes()
            if file.stem in ("ground-high", "ground-low", "top") and not unchanged:
                raise RuntimeError(
                    "valid original ground/top solar-disc images must remain byte-identical"
                )
            sun_rgb = expected - BACKGROUND
            relative = current_error / np.maximum(sun_rgb, 1e-5)
            rows.append(
                {
                    "case": file.stem,
                    "storage": storage,
                    "maximum_transfer_error": peak_t,
                    "previous_maximum_transfer_error": float(
                        np.max(old_error[interior]) * solid
                    ),
                    "solar_RGB_relative_max_with_1e5_floor": float(
                        relative[interior].max()
                    ),
                    "unchanged_image": unchanged,
                    "nonsolar_pixels_exact": int(np.count_nonzero(exterior)),
                    "interior_pixels": int(np.count_nonzero(interior)),
                    "sun_edge_pixels_unproven": int(
                        np.count_nonzero(~(interior | exterior))
                    ),
                    "native_disc_mask_equal": bool(
                        np.array_equal(
                            np.any(current[..., :3] != BACKGROUND, axis=-1),
                            np.any(previous[..., :3] != BACKGROUND, axis=-1),
                        )
                    ),
                }
            )
    if not all(
        row["previous_maximum_transfer_error"] > 0.1
        for row in rows
        if row["case"] == "high-orbit"
    ):
        raise RuntimeError(
            "the original outside-atmosphere mapping must fail the orbital control"
        )
    report = {
        "scope": "actual sky.frag and native RGBA16 production Veil; FP32 control promotes identical captured texels; fixed nonzero SkyView input; isolated1-degree solar-disc camera",
        "transfer_limit": TRANSFER_LIMIT,
        "sun_edge_roundoff_margin": float(SUN_EDGE_ROUNDOFF),
        "cases": rows,
    }
    (output / "analysis.json").write_text(json.dumps(report, indent=2))
    return report
