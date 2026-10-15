"""Actual ordinary source: continuous entrance cuts, closed/open pose and bounded costs."""

import argparse
import gc
import json
import resource
import time
import tracemalloc
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from demand import View
from entrance_clearance import clearance, collision, section
from evidence import revision
from facade_entrances import EntranceDetails
from facade_entry_geometry import products
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from outshine_ref.buildings.modelling import PrismVolume, subtract_prisms, union_prisms
from outshine_ref.buildings.opening_panels import OpeningPanel, panel_surface
from recess_probe import building
from showcase_tiles import view_for
from structural_shell_geometry import native
from verify_recess_consumer import camera, source_facades
from verify_urban_roofs import sources


def csg(plan):
    x0, z0, x1, z1 = plan.opening
    outer = PrismVolume(
        "panel", shapely.box(0.0, 0.0, plan.width_m, plan.depth_m), 0.0, plan.height_m
    )
    cut = PrismVolume("opening", shapely.box(x0, -1.0, x1, plan.depth_m + 1), z0, z1)
    mesh = dict(subtract_prisms("door-panel", union_prisms([outer]), [cut]).mesh)
    mesh["vertices"] = mesh["vertices"][:, [0, 2, 1]] * [1.0, 1.0, -1.0]
    if "normals" in mesh:
        mesh["normals"] = mesh["normals"][:, [0, 2, 1]] * [1.0, 1.0, -1.0]
    return mesh


def benchmark():
    rows = []
    for width, height in ((3.2, 3.0), (6.0, 4.0), (20.0, 4.0)):
        p = OpeningPanel(
            width, height, (width / 2 - 0.55, 0.16, width / 2 + 0.55, 2.31)
        )
        direct = panel_surface(p)
        other = csg(p)
        a = native({k: direct[k] for k in ("vertices", "faces")})
        b = native({k: other[k] for k in ("vertices", "faces")})
        difference = (a - b).volume() + (b - a).volume()
        if abs(difference) > 1e-9:
            raise ValueError(
                "Direct panel and existing CSG have different solid geometry"
            )
        for name, procedure in (
            ("direct", lambda p=p: panel_surface(p)),
            ("csg", lambda p=p: csg(p)),
        ):
            times = []
            for _ in range(21):
                t = time.perf_counter()
                value = procedure()
                times.append((time.perf_counter() - t) * 1000)
            del value
            gc.collect()
            tracemalloc.start()
            value = procedure()
            _, peak = tracemalloc.get_traced_memory()
            tracemalloc.stop()
            rows.append(
                {
                    "width_m": width,
                    "height_m": height,
                    "method": name,
                    "runs_ms": times,
                    "median_ms": float(np.median(times)),
                    "triangles": len(value["faces"]),
                    "array_bytes": sum(
                        v.nbytes for v in value.values() if isinstance(v, np.ndarray)
                    ),
                    "tracemalloc_peak_bytes": peak,
                    "solid_difference_m3": difference,
                    "work": "16 fixed quads, no boolean/topology backend"
                    if name == "direct"
                    else "2 prism meshes, 1 union, 1 difference; backend allocation work uninstrumented",
                }
            )
    return rows


def diagram(entry, output):
    opened = entry | {"angle_deg": 90.0}
    closed = entry | {"angle_deg": 0.0}
    centre, t = (
        np.asarray(entry["door"]["centre"]),
        np.asarray(entry["door"]["tangent"]),
    )
    inward = np.array([-t[1], t[0]])
    figure = plt.figure(figsize=(24, 8))
    for i, current in enumerate((closed, opened)):
        parts = products(current, 1, "through")
        solid = collision(parts).scale((1000.0,) * 3)
        cut = section(solid, 1.20)
        axis = figure.add_subplot(1, 3, i + 1)
        for p in shapely.get_parts(cut):
            xy = np.asarray(p.exterior.coords) - centre
            axis.fill(xy @ t, xy @ inward, color="#66818b")
        axis.plot([0, 0], [-0.8, 1.5], color="#d66b39", linewidth=3)
        axis.set(
            aspect="equal",
            xlim=(-2.2, 2.2),
            ylim=(-1.4, 2.0),
            xlabel="Tangente [m]",
            ylabel="Innenrichtung [m]",
            title=f"Tatsächlicher 1,2-m-Schnitt · {current['angle_deg']:.0f}°",
        )
    axis = figure.add_subplot(133, projection="3d")
    parts = products(opened, 1, "through")
    for part in parts[:5] + [parts[6]]:
        points = part["vertices"][part["faces"]].copy()
        xy = points[:, :, :2] - centre
        points[:, :, 0] = xy @ t
        points[:, :, 1] = xy @ inward
        axis.add_collection3d(
            Poly3DCollection(points, facecolors="#a9b7b5", edgecolors="none")
        )
    axis.set(
        xlim=(-2.2, 2.2),
        ylim=(-1.4, 2.0),
        zlim=(0, 3.2),
        title="2,5D · Eingangsteile, Ankunftsdecke/-fläche ausgeblendet",
    )
    axis.set_box_aspect((4.4, 3.4, 3.2))
    axis.view_init(elev=25, azim=-65)
    figure.tight_layout()
    figure.savefig(output / "entrance-plan-sections.png", dpi=120)
    plt.close(figure)
    return clearance(opened, parts)


def run(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze implementation before the entrance comparison")
    output.mkdir(parents=True, exist_ok=True)
    row = sources()[1] | {
        "layer": "building",
        "placement_base_enu_m": 0.0,
        "ground_asl_m": None,
        "placement_origin": "flat no DEM",
    }
    _, frames, p = source_facades(row)
    entry = EntranceDetails([row], shapely.STRtree([row["polygon"]])).plan(
        row, p, frames, "residential"
    )
    if entry is None:
        raise ValueError("Actual source entrance absent")
    c, t = np.asarray(entry["door"]["centre"]), np.asarray(entry["door"]["tangent"])
    n = np.array([t[1], -t[0]])
    target = np.r_[c, 1.30]
    near = View(
        np.r_[c + n * 8 + t * 3, 2.25], target, width=1920, height=1080, fov_deg=48
    )
    side = View(
        np.r_[c + n * 6 - t * 5, 2.25], target, width=1920, height=1080, fov_deg=48
    )
    scope = "Shared first-storey selector on one existing cell; same local leaf, true doorway, provisional arrival floor/ceiling. Other cells intact; full rooms/upper routes/external contact/native controller and radiance/LOD transition unproved."
    with patch("facade_entrances.EntranceDetails.plan", return_value=None):
        cases = [building(row, near, "Before", output, proof, scope=scope)]
    for name, view, angle in (
        ("Closed", near, 0.0),
        ("Open", near, 90.0),
        ("Side", side, 90.0),
        ("Overview", replace(view_for([row]), width=1920, height=1080), 0.0),
        ("Compact", camera(entry["frame"], 500), 0.0),
    ):
        cases.append(
            building(
                row,
                view,
                name,
                output,
                proof,
                scope=scope,
                door_states={entry["id"]: angle},
            )
        )
    for name in ("Closed", "Open", "Side"):
        data = json.loads((output / (name + "-geometry.json")).read_text())
        if not any(
            r["mode"] == "through" for r in data["procedures"]["entrance_geometry"]
        ):
            raise ValueError("Near entrance not actually attached")
    (output / "entrance-comparison.json").write_text(
        json.dumps(
            proof
            | {
                "cases": cases,
                "clearance": diagram(entry, output),
                "timings": benchmark(),
                "process_peak_rss_kib": resource.getrusage(
                    resource.RUSAGE_SELF
                ).ru_maxrss,
                "scope": "Local experiment; traced allocations omit backend memory; 2.5D explicitly omits arrival floor/ceiling, full assembly separately checked",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
