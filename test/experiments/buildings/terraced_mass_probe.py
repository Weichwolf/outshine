"""Actual plans/sections and matched bare mass solids; no completed game assets."""

import argparse
import json
from dataclasses import asdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from shapely.affinity import translate

from building_layout import frame
from building_plan_views import patch
from evidence import revision
from industrial_relief_probe import camera
from podium_probe import write
from stepped_source_probe import overview
from structural_shell_geometry import mesh_section
from terraced_mass_geometry import bands, physical, presentation
from terraced_mass_plan import plan
from terraced_mass_sources import sources


def diagrams(output, name, mass, product):
    figure, axes = plt.subplots(2, 2, figsize=(19.2, 10.8))
    original = shapely.from_wkb(mass.source_wkb)
    origin = np.asarray(original.centroid.coords[0])
    first = max(1, len(mass.storeys) // 3)
    indices = (0, first, len(mass.storeys) - 1)
    for axis, index in zip(axes.ravel()[:3], indices, strict=True):
        storey = mass.storeys[index]
        current = shapely.from_wkb(storey.footprint_wkb)
        actual = mesh_section(
            product.mesh, product.vertex_roots, (storey.bottom_m + storey.top_m) / 2
        )
        for geometry, colour, alpha in (
            (original, "#e0e4e7", 1),
            (current, "#adbdc5", 1),
            (shapely.from_wkb(storey.terrace_wkb), "#e5be85", 1),
            (shapely.from_wkb(mass.core.footprint_wkb), "#775e85", 0.6),
            (shapely.from_wkb(mass.core.opening_wkb), "#e3dcf0", 0.8),
        ):
            patch(
                axis,
                translate(geometry, *-origin),
                facecolor=colour,
                edgecolor="#53636b",
                alpha=alpha,
                linewidth=0.6,
            )
        for ring in (actual.exterior, *actual.interiors):
            axis.plot(*(np.asarray(ring.coords) - origin).T, color="#283c46", lw=1)
        path = np.asarray(mass.core.path)[:, :2] - origin
        axis.plot(*path.T, color="#5f366d", lw=1)
        axis.scatter(*(np.asarray(mass.entry.centre) - origin), color="#a04a3d", s=25)
        axis.set(
            aspect="equal",
            title=f"Geschoss {index}: Z={storey.bottom_m:.2f}–{storey.top_m:.2f}m · Schnitt aus echter Außenmasse",
            xlabel="Ost relativ [m]",
            ylabel="Nord relativ [m]",
        )
        axis.autoscale_view()
    axis = axes[1, 1]
    core = shapely.from_wkb(mass.core.footprint_wkb)
    centre = np.asarray(core.centroid.coords[0])
    direction = np.asarray(frame(original)[0][:2])
    reach = np.linalg.norm(np.ptp(shapely.get_coordinates(original), axis=0)) + 1
    line = shapely.LineString([centre - direction * reach, centre + direction * reach])
    for volume in bands(mass):
        crossing = volume.polygon.intersection(line)
        for part in shapely.get_parts(crossing):
            if part.geom_type != "LineString":
                continue
            lo, hi = (
                ((np.asarray(part.coords) - centre) @ direction).min(),
                ((np.asarray(part.coords) - centre) @ direction).max(),
            )
            patch(
                axis,
                shapely.box(lo, volume.low_m, hi, volume.high_m),
                facecolor="#adbdc5",
                edgecolor="#53636b",
                linewidth=1,
            )
    trace = np.asarray(mass.core.path)
    for i in range(len(mass.storeys) - 1):
        axis.plot(
            (trace[:, :2] - centre) @ direction,
            trace[:, 2] + i * mass.core.rise_m,
            color="#775e85",
            lw=1,
        )
    axis.set(
        aspect="equal",
        title="Massenprofil und reservierte Treppenlinien · kein Begehbarkeitsnachweis",
        xlabel="Schnittachse ab Kern [m]",
        ylabel="Z [m]",
    )
    axis.autoscale_view()
    figure.suptitle(
        mass.identifier
        + " · "
        + mass.kind
        + " · noch ohne Fenster, Hohlräume und Zugangsgeometrie"
    )
    figure.tight_layout()
    figure.savefig(output / (name + "-plan.png"), dpi=100)
    plt.close(figure)


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError(
            "Freeze the mass/core prototype before actual picture evidence"
        )
    output.mkdir(parents=True, exist_ok=True)
    records = []
    for index, row in enumerate(sources()):
        for kind in ("cascade", "paired-wings"):
            mass = plan(row, kind)
            product = physical(mass)
            name = str(index) + "-" + kind
            diagrams(output, name, mass, product)
            blueprint = json.loads(
                json.dumps(asdict(mass), default=lambda value: value.hex())
            )
            points = np.array(
                [
                    (x, y, z)
                    for x, y in shapely.get_coordinates(row["polygon"])
                    for z in (mass.bottom_m, mass.top_m)
                ]
            )
            view = overview(points, [1, -1.25, 1.25])
            for suffix, current in (
                ("Before", physical(mass, original=True)),
                ("After", product),
            ):
                write(
                    output,
                    name + suffix,
                    presentation(current),
                    {
                        "camera": camera(view),
                        "mass_plan": blueprint,
                        "source_input": {
                            key: value for key, value in row.items() if key != "polygon"
                        },
                        "source": {
                            "material_definitions": {
                                "12": {
                                    "color.r": 0.72,
                                    "color.g": 0.71,
                                    "color.b": 0.68,
                                    "roughness": 0.78,
                                    "metallic": 0,
                                }
                            }
                        },
                        "facade_materials": {},
                        "ground_span_m": 10000,
                        "scope": "Actual matched closed mass solids only. Original supplied footprint/height retained; no hollow interior, windows, actor access, native game asset or complete architecture-family admission.",
                    },
                    proof,
                )
            records.append(
                {
                    "id": mass.identifier,
                    "kind": kind,
                    "geometry": name,
                    "storeys": len(mass.storeys),
                    "triangles": len(product.mesh["faces"]),
                    "physical_mass_volume_m3": product.volume_m3,
                    "physical_mass_area_m2": product.area_m2,
                }
            )
            print(json.dumps(records[-1]), flush=True)
    (output / "plans.json").write_text(
        json.dumps(proof | {"records": records}, indent=2) + "\n"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == Path("/tmp") or not output.is_relative_to(Path("/tmp")):
        parser.error("--output requires a subdirectory of /tmp")
    models(output)


if __name__ == "__main__":
    main()
