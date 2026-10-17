"""Source section, projected support bay and plan before perspective image admission."""

import argparse
import json
from dataclasses import asdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from cooling_tower_mesh import beam
from cooling_tower_probe import sources
from evidence import revision
from recipe_graph_data import plain


def sections(output):
    proof = revision()
    _rows, _receipt, _view, cases = sources()
    output.mkdir(parents=True, exist_ok=True)
    for index, (row, programme) in enumerate(cases):
        fig, axes = plt.subplots(2, 2, figsize=(19.2, 10.8), dpi=100)
        original = np.asarray(programme.source.knots)
        shell = np.asarray(programme.shell.knots)
        for j in (1, 2):
            axes[0, 0].plot(original[:, j], original[:, 0], color="#8e9ca5", ls="--")
            axes[0, 0].plot(shell[:, j], shell[:, 0], color="#176f89")
        axes[0, 0].set(
            title="Source profile / raised shell · radial section",
            xlabel="radius (m)",
            ylabel="height (m)",
        )
        for name, p in (("rim", programme.rim),):
            values = np.asarray(p.knots)
            x = [values[0, 2], values[0, 1], values[-1, 1], values[-1, 2]]
            z = [values[0, 0], values[0, 0], values[-1, 0], values[-1, 0]]
            target = axes[1, 1]
            target.fill(x, z, alpha=0.7, label=name)
        low = programme.source.knots[0][0]
        axes[0, 1].fill_between(
            [-4, 4], low, low + 0.18, color="#607c68", label="foundation level"
        )
        for support in programme.supports[:2]:
            mesh = beam(support, 1)
            points = np.c_[
                mesh["vertices"][:, 1] - programme.source.centre_xy[1],
                mesh["vertices"][:, 2],
            ]
            hull = shapely.MultiPoint(points).convex_hull
            x, z = hull.exterior.xy
            axes[0, 1].fill(x, z, color="#a27c4a", alpha=0.6)
        axes[0, 1].axhline(
            programme.shell.knots[0][0], color="#176f89", label="shell start"
        )
        axes[0, 1].set(
            xlim=(-4, 4),
            ylim=(low - 0.2, low + 10),
            title="One bay · tangential projection of actual supports",
            xlabel="tangential distance (m)",
            ylabel="height (m)",
        )
        x, y = row["polygon"].exterior.xy
        axes[1, 0].plot(x, y, color="#8e9ca5", label="source outline")
        centre = np.asarray(programme.source.centre_xy)
        angles = np.linspace(0, 2 * np.pi, 300)
        ring = (
            centre
            + programme.source.knots[0][1] * np.c_[np.cos(angles), np.sin(angles)]
        )
        axes[1, 0].plot(*ring.T, color="#176f89", label="inscribed base")
        for support in programme.supports:
            a, b = np.asarray(support.start), np.asarray(support.end)
            axes[1, 0].plot([a[0], b[0]], [a[1], b[1]], color="#a27c4a", lw=0.7)
        axes[1, 0].set(
            title=f"Actual source plan · {programme.bays} bays / {len(programme.supports)} supports",
            xlabel="east (m)",
            ylabel="north (m)",
        )
        axes[1, 0].set_aspect("equal")
        axes[1, 1].plot(
            shell[-3:, 1], shell[-3:, 0], color="#176f89", label="outer shell"
        )
        axes[1, 1].plot(
            shell[-3:, 2], shell[-3:, 0], color="#176f89", ls="--", label="inner shell"
        )
        top, radius = original[-1, :2]
        axes[1, 1].set(
            xlim=(radius - 1.2, radius + 0.8),
            ylim=(top - 2, top + 0.2),
            title="Upper rim · radial section",
            xlabel="radius (m)",
            ylabel="height (m)",
        )
        for axis in axes.flat:
            axis.grid(alpha=0.2)
        axes[1, 0].legend()
        axes[1, 1].legend()
        fig.suptitle(
            f"Cooling intake {index} · source interval {original[0, 0]:g}–{original[-1, 0]:g} m · procedural construction",
            fontsize=16,
        )
        fig.tight_layout()
        fig.savefig(output / f"cooling-{index}-sections.png")
        plt.close(fig)
        (output / f"cooling-{index}-plan.json").write_text(
            json.dumps(
                proof | {"source_id": row["id"], "programme": asdict(programme)},
                default=plain,
                indent=2,
            )
            + "\n"
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    sections(args.output)
