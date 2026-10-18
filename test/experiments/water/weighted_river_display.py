"""Shared-axis whole 3D diagnostic of the prior, minimax and weighted river surfaces."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.colors import Normalize
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from verify_weighted_river import verify


def render(args):
    if not args.output.resolve().is_relative_to(Path("/tmp")) or args.output.exists():
        raise ValueError("fresh private /tmp diagnostic required")
    verify(args.products, args.input)
    panels = []
    with np.load(args.products / "weighted.npz") as saved:
        prior = saved["vertices"].copy()
        prior[:, 2] = saved["prior"]
        panels.append(("Same continuous prior", prior[saved["triangles"]]))
    for name, title in (
        ("minimax", "LP: minimum maximum error"),
        ("weighted", "QP: area-weighted vertex error"),
    ):
        with np.load(args.products / (name + ".npz")) as saved:
            panels.append((title, saved["vertices"][saved["triangles"]]))
    all_points = np.concatenate([x.reshape(-1, 3) for _, x in panels])
    low, high = all_points.min(0), all_points.max(0)
    span = np.maximum(high - low, 1)
    norm = Normalize(low[2], high[2])
    fig, axes = plt.subplots(
        1,
        3,
        figsize=(12.8, 7.2),
        dpi=100,
        layout="constrained",
        subplot_kw={"projection": "3d"},
    )
    for ax, (title, faces) in zip(axes, panels, strict=True):
        ax.add_collection3d(
            Poly3DCollection(
                faces,
                facecolors=plt.cm.viridis(norm(faces[:, :, 2].mean(1))),
                edgecolors="none",
            )
        )
        ax.set(
            xlim=(low[0], high[0]),
            ylim=(low[1], high[1]),
            zlim=(low[2] - 1, high[2] + 1),
            title=title,
            xlabel="East [m]",
            ylabel="North [m]",
            zlabel="ASL [m]",
        )
        ax.set_box_aspect((span[0], span[1], span[2] * 60))
        ax.set_proj_type("ortho")
        ax.view_init(elev=27, azim=-60)
    fig.suptitle(
        "Python water diagnostic, whole body / shared axes and colour / Z x60 / 1280x720"
    )
    fig.savefig(args.output, dpi=100)
    plt.close(fig)
    print(
        json.dumps(
            {
                "profile": [1280, 720],
                "vertical_exaggeration": 60,
                "scope": "Diagnostic geometry only; no engine screenshot or rendering-quality admission",
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    render(parser.parse_args())
