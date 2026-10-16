"""Matched cache-only fork views; camera metadata never changes plant geometry."""

import argparse
import json
from pathlib import Path

import numpy as np


def build(inputs, output, node):
    plans = json.loads((inputs / "collar-plans.json").read_text())
    selected = next(p for p in plans if p["fork_node"] == node)
    with np.load(inputs / "spatial-plan.npz") as z:
        points, parents = z["positions"], z["parents"]
    child = selected["branches"][1]["nodes"][1]
    radius = selected["span_m"] / 2.5
    incoming, outgoing = (
        points[node] - points[parents[node]],
        points[child] - points[node],
    )
    normal = np.cross(incoming, outgoing)
    if np.linalg.norm(normal) < 1e-10:
        normal = np.cross(incoming, np.eye(3)[np.argmin(abs(incoming))])
    normal /= np.linalg.norm(normal)
    centre = points[node] + (0, 0, radius * 0.12 / 0.14)
    view = {
        "eye": (centre + normal * (radius * 20) + (0, 0, radius * 0.6 / 0.14)).tolist(),
        "target": centre.tolist(),
        "span_m": radius * 1.2 / 0.14,
    }
    output.mkdir(parents=True, exist_ok=False)
    for label in ("recursive", "spatial"):
        folder = output / label
        folder.mkdir()
        for source in (inputs / label).iterdir():
            if source.is_file() and source.name != "entry.json":
                (folder / source.name).symlink_to(source.resolve())
        entry = json.loads((inputs / label / "entry.json").read_text())
        entry["preview_views"] = {"detail": view}
        (folder / "entry.json").write_text(json.dumps(entry, indent=2) + "\n")
    (output / "camera.json").write_text(
        json.dumps({"fork_node": node, "view": view}, indent=2) + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--node", type=int, required=True)
    args = parser.parse_args()
    build(args.inputs, args.output, args.node)
