"""Reproduce planar pool-contact controls without supplying native neighborhood heights."""

import argparse
import hashlib
import json
import os
import resource
import sys
from pathlib import Path

import numpy as np
from pool_body import build
from pool_body_experiment import FIXTURE, construct, original
from pool_contact_experiment import measured
from pool_terrain_contact import contact_patch


def controls(directory):
    data, source, _, water, _ = original(directory)
    body = construct(data, source, water)
    vertices, indices = np.unique(water.reshape(-1, 3), axis=0, return_inverse=True)
    low, high = (
        body["vertices"][:, :2].min(axis=0) - 1,
        body["vertices"][:, :2].max(axis=0) + 1,
    )
    rectangle = np.array(
        [
            [low[0], high[1], 1],
            [high[0], high[1], 1],
            [low[0], low[1], 1],
            [high[0], low[1], 1],
        ]
    )
    square_water = np.array(
        [[[0, 0, 0], [2, 0, 0], [2, 2, 0]], [[0, 0, 0], [2, 2, 0], [0, 2, 0]]]
    )
    square = build(
        square_water,
        lambda xy: np.zeros(len(xy)),
        kind="swimming_pool",
        ground_role="declared_ground",
        footprint_role="declared_complete_local_component",
        wall_width_m=0.25,
    )
    corner = np.array([[-1.25, -0.25, 0], [-0.25, -1.25, 0], [0.25, 0.25, 0]])
    return (
        ("original_water_facets", body, vertices, indices.reshape(-1, 3)),
        (
            "original_outline_rectangle",
            body,
            rectangle,
            np.array([[0, 2, 3], [0, 3, 1]]),
        ),
        ("analytic_corner", square, corner, np.array([[0, 1, 2]])),
    )


def display(records, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import shapely
    from matplotlib.colors import ListedColormap

    figure, axes = plt.subplots(1, 3, figsize=(15, 5), constrained_layout=True)
    for axis, (name, model, source, source_faces, vertices, faces) in zip(
        axes, records
    ):
        ring = model["plan_xy"][model["outer_ring"]]
        centre = vertices[faces, :2].mean(axis=1)
        inside = shapely.covers(shapely.polygons(ring), shapely.points(centre))
        axis.tripcolor(
            vertices[:, 0],
            vertices[:, 1],
            faces,
            facecolors=inside.astype(float),
            vmin=0,
            vmax=1,
            cmap=ListedColormap(["#e7dec9", "#bed9e8"]),
        )
        axis.triplot(vertices[:, 0], vertices[:, 1], faces, color="#527b95", lw=0.35)
        axis.triplot(
            source[:, 0], source[:, 1], source_faces, color="#7c623c", lw=0.65, ls="--"
        )
        loop = np.vstack((ring, ring[0]))
        axis.plot(loop[:, 0], loop[:, 1], color="black", lw=1.1)
        low, high = source[:, :2].min(axis=0), source[:, :2].max(axis=0)
        pad = max(high - low) * 0.05
        axis.set(
            xlim=(low[0] - pad, high[0] + pad),
            ylim=(low[1] - pad, high[1] + pad),
            xlabel="east / m",
            ylabel="north / m",
            title=name.replace("_", " ")
            + f"\n{len(vertices)} vertices / {len(faces)} triangles",
        )
        axis.set_aspect("equal")
    figure.suptitle(
        "Exact planar partition controls; original native terrain neighborhood remains pending"
    )
    figure.savefig(output / "planar-controls.png", dpi=150)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=FIXTURE)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=7)
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.repeats <= 21:
        parser.error("bounded repeat count between 1 and 21 required")
    args.output.mkdir(parents=True, exist_ok=True)
    records, reports = [], {}
    for name, model, source, source_faces in controls(args.input):
        vertices, faces, obligations, report = contact_patch(
            model, source, source_faces
        )
        np.savez(
            args.output / (name + ".npz"),
            source_vertices=source,
            source_faces=source_faces,
            vertices=vertices,
            faces=faces,
            obligations=obligations,
            pool_boundary=model["plan_xy"][model["outer_ring"]],
        )
        records.append((name, model, source, source_faces, vertices, faces))
        reports[name] = {
            **report,
            "cost": measured(model, source, source_faces, args.repeats),
        }
    report = {
        "source_pool_input_sha256": hashlib.sha256(
            (args.input / "input.json").read_bytes()
        ).hexdigest(),
        "scope": "Planar geometry controls using the existing pool outline and one analytic corner. Supplied control heights are not native terrain samples or a reconstructed DEM.",
        "cases": reports,
        "process_peak_rss_before_plot_bytes": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        "loadavg": os.getloadavg(),
    }
    if args.render:
        display(records, args.output)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "cases": len(records),
                "output": str(args.output),
                "scope": report["scope"],
            }
        )
    )


if __name__ == "__main__":
    main()
