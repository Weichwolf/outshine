"""Controlled full-triangle versus point/region visibility comparison (#844)."""

import argparse
import json
import tempfile
import time
from pathlib import Path

import numpy as np
from capture_region_visibility import covered_from_ball


def inputs(scale):
    blocker = np.array([[-12, -10, 10], [12, -10, 10], [0, 14, 10.0]])
    targets = np.array(
        [
            [[-1, -1, 30], [1, -1, 30], [0, 1, 30]],
            [[17, -6, 30], [17.25, -6, 30], [17, -5.75, 30]],
        ],
        dtype=float,
    )
    return blocker * scale, targets * scale


def render(triangles, eye, width=1280, height=720, focal=1044.33155):
    """Independent pinhole rays: intersect original triangle planes then edge tests."""
    yy, xx = np.mgrid[:height, :width]
    direction = np.c_[
        (xx.ravel() + 0.5 - width / 2) / focal,
        (height / 2 - yy.ravel() - 0.5) / focal,
        np.ones(width * height),
    ]
    distance = np.full(len(direction), np.inf)
    ids = np.full(len(direction), -1, np.int16)
    for index, triangle in triangles:
        a, b, c = triangle
        normal = np.cross(b - a, c - a)
        denominator = direction @ normal
        t = np.divide(
            (a - eye) @ normal,
            denominator,
            out=np.full(len(direction), np.inf),
            where=denominator != 0,
        )
        finite = np.isfinite(t) & (t > 0) & (t < distance)
        rays = np.flatnonzero(finite)
        hits = eye + t[rays, None] * direction[rays]
        inside = np.ones(len(hits), bool)
        for start, end in ((a, b), (b, c), (c, a)):
            inside &= np.cross(end - start, hits - start) @ normal >= 0
        accepted = rays[inside]
        distance[accepted], ids[accepted] = t[accepted], index
    return distance, ids.reshape(height, width)


def run(output):
    from PIL import Image, ImageDraw

    report = {
        "profile": {"width": 1280, "height": 720, "focal_px": 1044.33155},
        "cases": [],
    }
    palette = np.array(
        [[80, 110, 150], [70, 190, 100], [255, 120, 40], [18, 20, 25]], np.uint8
    )
    panel = Image.new("RGB", (1280 * 3, (720 + 28) * 4), "white")
    draw = ImageDraw.Draw(panel)
    for case, scale in enumerate((1.0, 100.0)):
        blocker, targets = inputs(scale)
        eye = np.zeros(3)
        began = time.perf_counter()
        point = [covered_from_ball(blocker, t, eye, 0, opaque=True) for t in targets]
        region = [covered_from_ball(blocker, t, eye, 6, opaque=True) for t in targets]
        predicate_ms = (time.perf_counter() - began) * 1000
        full = [(0, blocker), *enumerate(targets, 1)]
        variants = [
            full,
            [full[0]]
            + [p for p, r in zip(full[1:], point, strict=True) if not r["covered"]],
            [full[0]]
            + [p for p, r in zip(full[1:], region, strict=True) if not r["covered"]],
        ]
        record = {
            "scale": scale,
            "radius_m": 6,
            "point": point,
            "region": region,
            "four_predicates_wall_ms": predicate_ms,
            "views": [],
        }
        for pose, moved in enumerate((eye, np.array([6.0, 0, 0]))):
            reference, reference_ids = render(full, moved)
            counts = []
            for variant, triangles in enumerate(variants):
                distance, ids = (
                    render(triangles, moved) if variant else (reference, reference_ids)
                )
                both = np.isfinite(distance) & np.isfinite(reference)
                lost = np.isfinite(reference) & ~np.isfinite(distance)
                wrong = np.zeros(len(distance), bool)
                wrong[both] = distance[both] != reference[both]
                counts.append(
                    {
                        "lost_pixels": int(lost.sum()),
                        "wrong_depth_pixels": int(wrong.sum()),
                        "emitted_triangles": len(triangles),
                        "triangle_ray_tests": len(triangles) * 1280 * 720,
                    }
                )
                row = case * 2 + pose
                panel.paste(
                    Image.fromarray(palette[ids]), (variant * 1280, row * 748 + 28)
                )
                draw.text(
                    (variant * 1280 + 8, row * 748 + 5),
                    f"{('Full', 'Point omission', 'Ball omission')[variant]} / scale {scale:g} / eye X {moved[0]:g}m / lost {int(lost.sum())} / depth {int(wrong.sum())}",
                    fill="black",
                )
            assert counts[2]["lost_pixels"] == counts[2]["wrong_depth_pixels"] == 0
            record["views"].append(counts)
        report["cases"].append(record)
        np.savez_compressed(
            output / f"input-{case}.npz",
            blocker=blocker,
            targets=targets,
            eye=eye,
            radius_m=6,
        )
    report["scope"] = (
        "Synthetic opaque triangles, exact Float64-input predicates; original triangles retained on uncertainty; no native patch, material, shadow, whole-scene or device admission. Radius 6m is the requested movement control, not a world constant."
    )
    report["cost_scope"] = (
        "13 exact plane tests at most per pair; Python Fraction allocations and bit arithmetic unmeasured; renderer work counts all dense triangle/ray pairs, not GPU or cache traffic."
    )
    panel.save(output / "comparison.png")
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private output belongs in system temp")
    output.mkdir(parents=True, exist_ok=False)
    run(output)


if __name__ == "__main__":
    main()
