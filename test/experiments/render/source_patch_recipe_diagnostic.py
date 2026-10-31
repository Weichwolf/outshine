"""Inspect original and reconstructed undersides in the same source frame.

False colours show normal-up components only; this is not a PBR/raster image.
"""

import argparse
import hashlib
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from experiment_output import create_output
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def draw(args):
    create_output(args.output)
    identity = json.loads((args.input / "sample.json").read_text())
    with np.load(args.input / "sample.npz", allow_pickle=False) as saved:
        old = saved["original_vertices"][saved["original_faces"]]
        new = saved["candidate_vertices"][saved["candidate_faces"]]
    u = new[..., 6]
    floor = np.all((u < 0) & (np.fmod(-u - 1, 16) == 8), axis=1)
    panels = (old, new[~floor], new)
    center = np.concatenate(
        (old[..., :3].reshape(-1, 3), new[..., :3].reshape(-1, 3))
    ).mean(0)
    transformed = [(p[..., [0, 2, 1]] - center[[0, 2, 1]]) * [1, -1, 1] for p in panels]
    all_points = np.concatenate([p.reshape(-1, 3) for p in transformed])
    low, high = all_points.min(0), all_points.max(0)
    figure = plt.figure(figsize=(13, 5), layout="constrained")
    cmap = plt.get_cmap("coolwarm")
    for i, (vertices, positions, title) in enumerate(
        zip(
            panels,
            transformed,
            (
                "Native original",
                "Roof/wall domains without floor",
                "Complete source domains",
            ),
            strict=True,
        ),
        1,
    ):
        ax = figure.add_subplot(1, 3, i, projection="3d")
        collection = Poly3DCollection(
            positions,
            facecolors=cmap((vertices[..., 4].mean(1) + 1) / 2),
            edgecolors="black",
            linewidths=0.35,
        )
        ax.add_collection3d(collection)
        ax.set(
            xlim=(low[0], high[0]),
            ylim=(low[1], high[1]),
            zlim=(low[2], high[2]),
            xlabel="Source east [m]",
            ylabel="Source north [m]",
            zlabel="Source up [m]",
            title=f"{title}\n{len(vertices)} triangles",
        )
        ax.set_box_aspect(np.maximum(high - low, 0.001))
        ax.view_init(elev=-25, azim=-60)
    figure.suptitle(
        f"Prepared source {identity['prepared_source_ordinal']}: underside construction diagnostic, not material/raster/World admission"
    )
    figure.savefig(args.output / "underside.png", dpi=130)
    plt.close(figure)
    (args.output / "inputs.json").write_text(
        json.dumps(
            {
                name: hashlib.sha256((args.input / name).read_bytes()).hexdigest()
                for name in ("sample.json", "sample.npz", "report.json")
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    draw(parser.parse_args())
