"""Source facade band profiles: cuts, 2.5D, actual near/opposite/far and equal-geometry costs."""

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
from building_mesh import slice_triangles
from evidence import revision
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from outshine_ref.buildings.profiled_bands import BandPlan, band_surface
from recess_probe import building
from showcase_tiles import view_for
from tile_facade_bands import specifications
from verify_recess_consumer import source_facades
from verify_urban_roofs import sources


def benchmark():
    records = []
    profile = ((0.0, 0.0), (0.08, 0.0), (0.2, 0.12), (0.2, 0.3), (0.0, 0.3))
    for count in (4, 64, 256):
        angles = np.arange(count) * (2 * np.pi / count)
        polygon = shapely.Polygon(np.c_[np.cos(angles), np.sin(angles)] * 20)
        plan = BandPlan(polygon, profile)
        expected = band_surface(plan, method="scalar")
        for method in ("scalar", "batch"):
            timings = []
            for _ in range(21):
                start = time.perf_counter()
                actual = band_surface(plan, method=method)
                timings.append((time.perf_counter() - start) * 1000)
            for key in expected:
                np.testing.assert_array_equal(expected[key], actual[key])
            del actual
            gc.collect()
            tracemalloc.start()
            actual = band_surface(plan, method=method)
            _, peak = tracemalloc.get_traced_memory()
            tracemalloc.stop()
            records.append(
                {
                    "edges": count,
                    "method": method,
                    "median_ms": float(np.median(timings)),
                    "runs_ms": timings,
                    "array_bytes": sum(v.nbytes for v in actual.values()),
                    "tracemalloc_peak_bytes": peak,
                    "triangles": len(actual["faces"]),
                    "xy_array_allocations": 1 if method == "batch" else count + 1,
                    "xy_materialized_bytes": count
                    * len(profile)
                    * 4
                    * 2
                    * 8
                    * (1 if method == "batch" else 2),
                }
            )
    return records


def diagram(row, output):
    _, facades, p = source_facades(row)
    specs = specifications(facades, p["minHeight"])
    figure = plt.figure(figsize=(24, 8))
    plan = figure.add_subplot(131)
    section = figure.add_subplot(132)
    spatial = figure.add_subplot(133, projection="3d")
    colours = ("#829093", "#d2c4ad", "#b4a48c", "#eee3cf")
    for colour, spec in zip(colours, specs, strict=False):
        source = BandPlan(row["polygon"], spec.profile, spec.base_m)
        mesh = band_surface(source)
        for line, _ in slice_triangles(
            mesh | {"materials": np.zeros(len(mesh["faces"]), dtype=int)},
            [0, 0, 1],
            spec.base_m + spec.profile[-1][1] * 0.5,
        ):
            plan.plot(line[:, 0], line[:, 1], color=colour, linewidth=2)
        profile = np.asarray(spec.profile)
        section.fill(
            profile[:, 0], profile[:, 1] + spec.base_m, color=colour, label=spec.kind
        )
        spatial.add_collection3d(
            Poly3DCollection(
                mesh["vertices"][mesh["faces"]], facecolors=colour, edgecolors="none"
            )
        )
    polygon = row["polygon"]
    for ring in (polygon.exterior, *polygon.interiors):
        points = np.asarray(ring.coords)
        plan.plot(points[:, 0], points[:, 1], color="black", linewidth=0.6)
    first = facades[0]
    floor = first["height_m"] / first["rows"]
    for i in range(first["rows"]):
        low, high = floor * (i + 0.5 - np.array([-1, 1]) * first["fractions"][1] / 2)
        section.plot([-0.03, -0.03], [low, high], color="#167dac", linewidth=5)
    plan.set(
        aspect="equal",
        title="Tatsächliche Quellschnitte · XY [m]",
        xlabel="East [m]",
        ylabel="North [m]",
    )
    section.set(
        title="Fensterfreie Profilhöhen", xlabel="Ausladung [m]", ylabel="Höhe [m]"
    )
    section.legend()
    x0, y0, x1, y1 = polygon.bounds
    spatial.set(
        xlim=(x0 - 0.3, x1 + 0.3),
        ylim=(y0 - 0.3, y1 + 0.3),
        zlim=(0, first["height_m"]),
        title="2,5D · tatsächliche Profilkörper",
    )
    spatial.set_box_aspect((x1 - x0, y1 - y0, first["height_m"]))
    spatial.view_init(elev=25, azim=-65)
    figure.tight_layout()
    figure.savefig(output / "band-plan-sections.png", dpi=120)
    plt.close(figure)


def run(output):
    provenance = revision()
    if provenance["experiment_tree_dirty"]:
        raise ValueError("Freeze source before generating profile comparisons")
    output.mkdir(parents=True, exist_ok=True)
    row = sources()[1] | {
        "layer": "building",
        "placement_base_enu_m": 0.0,
        "ground_asl_m": None,
        "placement_origin": "flat no DEM",
    }
    diagram(row, output)
    near = replace(view_for([row]), width=1920, height=1080)
    opposite = replace(near, eye=near.target + (near.eye - near.target) * [-1, -1, 1])
    far = replace(near, eye=near.target + (near.eye - near.target) * 100)
    scope = "Actual source and unchanged opening plan; procedural profile bands in existing repeat gaps. Flat/no DEM. No entrance, full architecture-family, radiance/shadow/LOD/native/device admission."
    with patch(
        "tile_facade_bands.BandDetails.__call__",
        side_effect=lambda mesh, *args, **kwargs: mesh,
    ):
        cases = [building(row, near, "Before", output, provenance, scope=scope)]
    for name, view in (("Near", near), ("Opposite", opposite), ("Far", far)):
        cases.append(building(row, view, name, output, provenance, scope=scope))
    for name in ("Near", "Opposite"):
        report = json.loads((output / (name + "-geometry.json")).read_text())
        if not report["procedures"]["bands"]:
            raise ValueError("Actual source near bands absent")
    far_report = json.loads((output / "Far-geometry.json").read_text())
    if far_report["procedures"]["bands"]:
        raise ValueError("Far band expansion")
    (output / "band-comparison.json").write_text(
        json.dumps(
            provenance
            | {
                "cases": cases,
                "timings": benchmark(),
                "process_peak_rss_kib": resource.getrusage(
                    resource.RUSAGE_SELF
                ).ru_maxrss,
                "scope": "Local equal-array scalar/batch comparison, no runtime/radiance/shadow/LOD/device admission; miter-limited sphere projection bound. Physical entrances remain unresolved.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
