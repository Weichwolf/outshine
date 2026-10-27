"""Saved actual profile geometry and grade comparisons (#1533)."""

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import FormatStrFormatter
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def mesh(axis, rings, limits):
    lower = np.arange(len(rings) - 1)[:, None] * 4 + np.arange(3)
    faces = np.stack(
        (lower, lower + 4, lower + 1, lower + 1, lower + 4, lower + 5), axis=-1
    ).reshape(-1, 3)
    triangles = rings.reshape(-1, 3)[faces]
    normal = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    length = np.linalg.norm(normal, axis=1)
    normal /= np.where(length > 0, length, 1)[:, None]
    light = np.array([0.4, -0.2, 0.9])
    light /= np.linalg.norm(light)
    brightness = 0.15 + 0.75 * np.maximum(0, normal @ light)
    color = np.c_[brightness, brightness, brightness, np.ones(len(brightness))]
    axis.add_collection3d(Poly3DCollection(triangles, facecolor=color, linewidth=0))
    axis.set_xlim(*limits[:, 0])
    axis.set_ylim(*limits[:, 1])
    axis.set_zlim(*limits[:, 2])
    axis.set_box_aspect(np.maximum(limits[1] - limits[0], 1e-6).copy())
    axis.set_proj_type("ortho")
    axis.view_init(elev=24, azim=-55)
    axis.set_zticks(limits[:, 2])
    axis.zaxis.set_major_formatter(FormatStrFormatter("%.3f"))
    axis.grid(False)
    axis.set_xlabel("East [m]")
    axis.set_ylabel("North [m]")
    axis.set_zlabel("Height [m]")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--grade", type=float, required=True)
    parser.add_argument("--witnesses", type=int, nargs=2, default=(4013, 55171))
    parser.add_argument("--after-label", default="Constrained cubic-crossfall recipe")
    parser.add_argument(
        "--title",
        default="Actual fixed-source sweeps: unchanged plan/height/outer sections",
    )
    args = parser.parse_args()
    data = np.load(args.input, allow_pickle=False)
    fig = plt.figure(figsize=(15, 9), layout="constrained")
    for i, ordinal in enumerate(args.witnesses):
        prefix = f"witness_{ordinal}_"
        original, after = data[prefix + "original"], data[prefix + "corrected"]
        joined = np.concatenate((original.reshape(-1, 3), after.reshape(-1, 3)))
        limits = np.array([joined.min(axis=0), joined.max(axis=0)])
        for j, (points, title) in enumerate(
            (
                (original, "Original angular-bank model"),
                (after, args.after_label),
            )
        ):
            ax = fig.add_subplot(2, 3, 3 * i + j + 1, projection="3d")
            mesh(ax, points, limits)
            ax.set_title(f"{ordinal}: {title}", fontsize=10)
        ax = fig.add_subplot(2, 3, 3 * i + 3)
        s = data[prefix + "stations"]
        old_grade = data[prefix + "original_grade"].max(axis=1)
        new_grade = data[prefix + "corrected_grade"].max(axis=1)
        ax.plot(s, old_grade, label=f"original (sample max {old_grade.max():.4f})")
        ax.plot(s, new_grade, label=f"corrected (sample max {new_grade.max():.4f})")
        ax.axhline(
            args.grade, color="black", ls="--", label="explicit experiment grade limit"
        )
        ax.set_xlabel("Reference station [m]")
        ax.set_ylabel("Maximum absolute edge grade")
        ax.legend(fontsize=8)
        ax.grid(alpha=0.2)
    fig.suptitle(
        args.title
        + "\nUnexaggerated geometry, Lambert diagnostic; limits are experiment inputs, no native/World/PBR admission."
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.output, dpi=120)
    plt.close(fig)


if __name__ == "__main__":
    main()
