"""Whole main skeleton and one real secondary fan, without foliage admission."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from spruce_branch_planes import lines, sparse_poses


def plot(inputs, output):
    rows = json.loads((inputs / "preflight.json").read_text())
    fig, panels = plt.subplots(2, 4, figsize=(16, 9), dpi=160)
    right = np.array((70.0, 18, 0))
    right /= np.linalg.norm(right)
    for column, row in enumerate(rows):
        main, branches, _ = sparse_poses(
            json.loads((inputs / (row["name"] + ".json")).read_text())
        )
        top, bottom = panels[:, column]
        for pose in main:
            points = lines(pose)
            top.plot(points @ right, points[:, 2], color="#357c4d", lw=0.45)
        top.set(xlim=(-6, 6), ylim=(0, 35), title=row["name"], ylabel="height m")
        top.set_aspect("equal")
        parent, shoots = branches[-1]
        for pose in [parent, *shoots]:
            points = (lines(pose) - parent.origin) @ parent.frame
            bottom.plot(
                points[:, 0],
                points[:, 1],
                color="#775232" if pose is parent else "#357c4d",
                lw=0.65,
            )
        bottom.set(
            xlim=(-1.7, 1.7),
            ylim=(0, 5),
            xlabel="parent lateral m",
            ylabel="parent forward m",
        )
        bottom.set_aspect("equal")
        top.grid(alpha=0.15)
        bottom.grid(alpha=0.15)
    fig.suptitle(
        "Picea abies | actual primary axes and bottom fan | NO foliage/material/shadow judgement"
    )
    fig.tight_layout()
    fig.savefig(output)
    plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    plot(args.inputs, args.output)
