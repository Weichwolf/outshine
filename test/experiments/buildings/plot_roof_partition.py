"""Private geometric roof comparison from saved plans; no product render (#955)."""

import argparse
import json
from pathlib import Path

import matplotlib
import numpy as np
import shapely

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


def source_plan(row):
    expected = row["expected"]
    faces = expected["facets"]
    return {
        "xy": np.array(expected["positions_local_xy_m"]),
        "distance": np.array(expected["wavefront_distances_m"]),
        "origin": np.array(row["frame"]["origin_xy_m"]),
        "offsets": np.r_[0, np.cumsum([len(f["nodes"]) for f in faces])],
        "indices": np.array([i for f in faces for i in f["nodes"]]),
        "owners": np.array([f["source_edge"] for f in faces]),
        "planes": np.array([[*f["gradient"], f["intercept_m"]] for f in faces]),
    }


def triangles(plan, origin):
    result, colours = [], []
    factor = np.tan(np.deg2rad(30.0))
    for index, (start, end) in enumerate(
        zip(plan["offsets"][:-1], plan["offsets"][1:], strict=True)
    ):
        polygon = shapely.Polygon(plan["xy"][plan["indices"][start:end]])
        parts = shapely.get_parts(shapely.constrained_delaunay_triangles(polygon))
        xy = shapely.get_coordinates(parts).reshape(-1, 4, 2)[:, :3]
        plane = plan["planes"][index]
        z = (xy @ plane[:2] + plane[2]) * factor
        result.extend(np.dstack((xy + plan["origin"] - origin, z)))
        owner = plan["owners"][index]
        colour = plt.get_cmap("tab20")((int(owner[0]) * 7 + int(owner[1])) % 20)
        colours.extend([colour] * len(xy))
    return np.array(result), colours


def plot(args):
    fixture = json.loads(args.fixture.read_text())
    selected = (
        "Vienna:1210000:12",
        "Vienna:776760:165",
        "Vienna:800890:10",
        "Tokyo:17970970:291",
    )
    fig = plt.figure(figsize=(12, 16), layout="constrained")
    with np.load(args.output / "plans.npz") as data:
        for row_number, name in enumerate(selected):
            index, row = next(
                (i, r) for i, r in enumerate(fixture["records"]) if r["id"] == name
            )
            reference = source_plan(row)
            candidate = {k: data[f"case_{index:02d}_{k}"] for k in reference}
            origin = reference["origin"]
            domain = np.array(row["outer"]) - origin
            lo, hi = domain.min(axis=0), domain.max(axis=0)
            centre = (lo + hi) * 0.5
            half = max(hi - lo) * 0.55
            rise = float(reference["distance"].max() * np.tan(np.deg2rad(30.0)))
            for column, (label, plan) in enumerate(
                (("Pinned original", reference), ("Own contour reference", candidate))
            ):
                ax = fig.add_subplot(4, 2, row_number * 2 + column + 1, projection="3d")
                faces, colours = triangles(plan, origin)
                ax.add_collection3d(
                    Poly3DCollection(
                        faces, facecolors=colours, edgecolors="#303438", linewidths=0.3
                    )
                )
                ax.set(
                    xlim=(centre[0] - half, centre[0] + half),
                    ylim=(centre[1] - half, centre[1] + half),
                    zlim=(0, rise * 1.2),
                )
                ax.set_box_aspect((half * 2, half * 2, rise * 1.2))
                ax.view_init(elev=43, azim=-62)
                ax.set_title(name + "\n" + label + " — same30° pitch", fontsize=10)
                ax.set_xticks([])
                ax.set_yticks([])
                ax.set_zticks([])
    fig.suptitle(
        "Original eaves/facets/courtyards — geometry only, diagnostic colours, no PBR/device admission"
    )
    fig.savefig(args.output / "roofs.png", dpi=130)
    plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fixture",
        type=Path,
        default=Path(__file__).with_name("reference") / "hip-plans.json",
    )
    parser.add_argument("--output", type=Path, required=True)
    plot(parser.parse_args())
