"""Expose bounding-box filter error against exact rotated finite facade areas."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely
from evidence import revision
from outshine_ref.buildings.facade_finish import CHANNELS, FacadeFinish
from outshine_ref.buildings.facades import FacadeRepeat


def regions(repeat, finish):
    """Explicit constructive regions, independent of periodic integral primitives."""
    cell = np.asarray(repeat.cell_size_m)
    half = cell * [repeat.opening_width, repeat.opening_height] / 2
    opening, glass = [], []
    for column in range(repeat.columns):
        for row in range(repeat.rows):
            centre = cell * [column + 0.5, row + 0.5]
            opening.append(shapely.box(*(centre - half), *(centre + half)))
            inner = np.maximum(half - finish.frame_m, 0)
            if np.all(inner > 0):
                glass.append(shapely.box(*(centre - inner), *(centre + inner)))
    opening, glass = shapely.union_all(opening), shapely.union_all(glass)
    frame = opening.difference(glass)
    half_band = min(finish.band_m, cell[1] - 2 * half[1]) / 2
    band = shapely.union_all(
        [
            shapely.box(
                0,
                max(0, row * cell[1] - half_band),
                repeat.width_m,
                min(repeat.height_m, row * cell[1] + half_band),
            )
            for row in range(repeat.rows + 1)
        ]
    )
    if max(frame.intersection(glass).area, opening.intersection(band).area) > 1e-10:
        raise ValueError("Independent material regions overlap")
    return frame, glass, band


def polygon(centre, dx, dy):
    if abs(np.linalg.det(np.column_stack((dx, dy)))) < 1e-12:
        raise ValueError("Area reference excludes rank-deficient footprints")
    return shapely.Polygon(
        [
            centre + sx * dx / 2 + sy * dy / 2
            for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))
        ]
    )


def integrate(fields, centre, dx, dy):
    pixel = polygon(centre, dx, dy)
    parts = np.array([field.intersection(pixel).area / pixel.area for field in fields])
    return np.r_[1 - parts.sum(), parts]


def run(output):
    provenance = revision()
    if provenance["experiment_tree_dirty"]:
        raise ValueError("Footprint evidence requires a clean frozen implementation")
    repeat = FacadeRepeat(20.0, 15.0, 6, 5, 0.48, 0.53)
    finish = FacadeFinish()
    fields = regions(repeat, finish)
    try:
        polygon(np.zeros(2), np.array([1.0, 0.0]), np.array([2.0, 0.0]))
    except ValueError:
        pass
    else:
        raise ValueError("Singular footprint was incorrectly treated as area")
    centres = np.array(
        [
            [-0.015, 3.02],
            [0.015, 3.02],
            [19.985, 14.985],
            [20.015, 15.015],
            [1.6666666667, 1.5],
            [0.82, 0.72],
        ]
    )
    rows = []
    for widths in ((0.1, 3.0), (0.6, 0.6), (3.0, 0.1)):
        for rotation in (0, 15, 45, 80):
            for opening in (5, 90, 175):
                theta = np.deg2rad(rotation)
                beta = theta + np.deg2rad(opening)
                dx = widths[0] * np.array([np.cos(theta), np.sin(theta)])
                dy = widths[1] * np.array([np.cos(beta), np.sin(beta)])
                box = np.maximum(np.abs(dx) + np.abs(dy), 1e-5)
                for centre in centres:
                    exact = integrate(fields, centre, dx, dy)
                    actual = finish.coverage(repeat, centre, box)
                    if min(exact.min(), actual.min()) < -1e-10:
                        raise ValueError("Unbounded material fraction")
                    np.testing.assert_allclose(
                        [exact.sum(), actual.sum()], 1, atol=1e-10
                    )
                    for other_dx, other_dy in ((-dx, dy), (dx, -dy), (dy, dx)):
                        np.testing.assert_allclose(
                            integrate(fields, centre, other_dx, other_dy),
                            exact,
                            rtol=1e-9,
                            atol=1e-10,
                        )
                    if rotation == 0 and opening == 90:
                        np.testing.assert_allclose(actual, exact, atol=1e-10)
                    rows.append(
                        {
                            "centre_m": centre.tolist(),
                            "dx_m": dx.tolist(),
                            "dy_m": dy.tolist(),
                            "rotation_deg": rotation,
                            "opening_deg": opening,
                            "box": actual.tolist(),
                            "parallelogram": exact.tolist(),
                            "max_absolute_error": float(np.max(np.abs(actual - exact))),
                        }
                    )
    worst = max(rows, key=lambda row: row["max_absolute_error"])
    report = provenance | {
        "channels": CHANNELS,
        "cases": len(rows),
        "worst": worst,
        "rows": rows,
        "scope": "Exact independent polygon intersections versus axis-aligned bounding-box facade filter. Nonzero determinant only; no shaded, temporal, native or GPU error bound.",
    }
    output.mkdir(parents=True, exist_ok=True)
    (output / "footprints.json").write_text(json.dumps(report, indent=2) + "\n")
    draw(output, fields, worst)
    print(json.dumps({"cases": len(rows), "worst": worst}))


def draw(output, fields, worst):
    import matplotlib.pyplot as plt
    from matplotlib.patches import Polygon as Patch

    figure, axes = plt.subplots(1, 2, figsize=(16, 9), layout="constrained")
    colours = ["#7d7e7b", "#424b55", "#7898ac", "#a5a6a0"]
    for field, colour in zip(fields, colours[1:], strict=True):
        for part in shapely.get_parts(field):
            axes[0].add_patch(Patch(np.asarray(part.exterior.coords), color=colour))
    centre = np.asarray(worst["centre_m"])
    dx, dy = np.asarray(worst["dx_m"]), np.asarray(worst["dy_m"])
    pixel = polygon(centre, dx, dy)
    axes[0].add_patch(
        Patch(
            np.asarray(pixel.exterior.coords), fill=False, edgecolor="red", linewidth=2
        )
    )
    box = np.abs(dx) + np.abs(dy)
    bound = shapely.box(*(centre - box / 2), *(centre + box / 2))
    axes[0].add_patch(
        Patch(
            np.asarray(bound.exterior.coords),
            fill=False,
            edgecolor="black",
            linestyle="--",
        )
    )
    axes[0].set(
        xlim=(centre[0] - 2, centre[0] + 2),
        ylim=(centre[1] - 2, centre[1] + 2),
        xlabel="Facade x [m]",
        ylabel="Facade y [m]",
        title="Red pixel parallelogram / dashed bounding box",
    )
    axes[0].set_aspect("equal")
    x = np.arange(4)
    axes[1].bar(x - 0.18, worst["parallelogram"], 0.36, label="Exact pixel area")
    axes[1].bar(x + 0.18, worst["box"], 0.36, label="Current UV box")
    axes[1].set(
        xticks=x,
        xticklabels=CHANNELS,
        ylim=(0, 1),
        ylabel="Material area fraction",
        title=f"Largest component error: {worst['max_absolute_error']:.4f}",
    )
    axes[1].legend()
    figure.suptitle(
        "Finite facade: adversarial rotated / thin footprints (not a Place rendering)"
    )
    figure.savefig(output / "footprints.png", dpi=120)
    plt.close(figure)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    run(parser.parse_args().output)
