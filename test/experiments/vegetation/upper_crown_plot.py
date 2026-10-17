"""Actual bounded primary-axis diagrams, never a foliage or render admission."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from outshine_ref.vegetation.curve_frame import arc_frame
from parent_profile_graph import ParentProfileGraph


def plot(inputs, output):
    rows = json.loads((inputs / "preflight.json").read_text())
    fig, panels = plt.subplots(2, 4, figsize=(16, 9), dpi=160, sharex=True, sharey=True)
    right = np.array((70.0, 18, 0))
    right /= np.linalg.norm(right)
    for row, panel in zip(rows, panels.flat, strict=True):
        graph = ParentProfileGraph.from_recipe(
            json.loads((inputs / (row["name"] + ".json")).read_text())
        )
        for key in (1, 2):
            for pose in graph.instances(key, 182):
                centre, *_ = arc_frame(pose.length, pose.bend, 0, np.linspace(0, 1, 16))
                points = centre @ pose.frame.T + pose.origin
                panel.plot(
                    points @ right,
                    points[:, 2],
                    color="#504b36" if key == 1 else "#397d4f",
                    lw=1 if key == 1 else 0.45,
                )
        panel.set_title(
            row["name"]
            + (
                "\nREJECTED"
                if row.get("rejected") or not row.get("area_admitted")
                else "\npreflight fits"
            ),
            fontsize=9,
        )
        panel.set(xlim=(-5, 5), ylim=(0, 35), xlabel="projected metres")
        panel.set_aspect("equal")
        panel.grid(alpha=0.15)
    panels[0, 0].set_ylabel("height m")
    panels[1, 0].set_ylabel("height m")
    fig.suptitle(
        "Picea abies | same180primary axes, actual poses | NO foliage/material/shadow judgement"
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
