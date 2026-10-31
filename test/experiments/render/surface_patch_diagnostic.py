"""Plot a saved exact fan comparison; geometry diagnostic, not a World rendering."""

import argparse
import hashlib
import json
import math
from fractions import Fraction as F
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from experiment_output import create_output
from matplotlib.collections import PolyCollection
from surface_patch_overlay import clip, twice_area, value


def draw(args):
    create_output(args.output)
    saved = json.loads(args.input.read_text())
    positions = np.array(saved["positions"])
    axes = saved["axes"]
    rows = [tuple(F(float(x)) for x in row) for row in positions]
    points = [tuple(row[i] for i in axes) for row in rows]
    faces = [saved["original_faces"], saved["replacement_faces"]]
    triangles = []
    for mesh in faces:
        chart = []
        for face in mesh:
            p, r = [points[i] for i in face], [rows[i] for i in face]
            if (p[1][0] - p[0][0]) * (p[2][1] - p[0][1]) < (p[1][1] - p[0][1]) * (
                p[2][0] - p[0][0]
            ):
                p.reverse()
                r.reverse()
            chart.append((p, r))
        triangles.append(chart)
    polygons, upper_mm = [], []
    for p, r in triangles[0]:
        for q, s in triangles[1]:
            intersection = clip(p, q)
            if not twice_area(intersection):
                continue
            polygons.append([[float(x) for x in point] for point in intersection])
            squared = max(
                sum(
                    (a - b) ** 2
                    for a, b in zip(value(p, r, x), value(q, s, x), strict=True)
                )
                for x in intersection
            )
            upper_mm.append(1000 * math.sqrt(float(squared)))
    figure, panels = plt.subplots(1, 3, figsize=(13, 5), layout="constrained")
    xy = positions[:, axes]
    for panel, mesh, title in zip(
        panels[:2], faces, ("Original", "Proposed boundary fan"), strict=True
    ):
        panel.triplot(xy[:, 0], xy[:, 1], np.array(mesh), linewidth=1)
        panel.set_title(f"{title}: {len(mesh)} triangles")
    collection = PolyCollection(
        polygons, array=np.array(upper_mm), edgecolors="black", linewidths=0.5
    )
    panels[2].add_collection(collection)
    panels[2].autoscale_view()
    panels[2].set_title("Whole-cell geometric upper bound")
    figure.colorbar(collection, ax=panels[2], label="Correspondence distance [mm]")
    for panel in panels:
        panel.set_aspect("equal")
        panel.set_xlabel(f"Source axis {axes[0]} [m]")
        panel.set_ylabel(f"Source axis {axes[1]} [m]")
        panel.ticklabel_format(useOffset=False)
    figure.suptitle(
        "Actual original surface: construction/error diagnostic; no material, raster or World admission"
    )
    figure.savefig(args.output / "comparison.png", dpi=130)
    plt.close(figure)
    (args.output / "input.sha256").write_text(
        hashlib.sha256(args.input.read_bytes()).hexdigest() + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    draw(parser.parse_args())
