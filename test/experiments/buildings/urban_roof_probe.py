"""Actual urban hip coverage, roof/body sections and incremental flat-envelope costs."""

import argparse
import json
import resource
import time
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from evidence import revision
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from outshine_ref.buildings.modelling import heightfield_envelope
from provider_contract import attributes
from structural_shell_geometry import mesh_section
from tile_buildings import prism
from tile_roof_envelopes import choose, roof_surface
from verify_roof_envelopes import case
from verify_urban_roofs import sources


def outline(axis, polygon, origin, **style):
    for ring in (polygon.exterior, *polygon.interiors):
        points = np.asarray(ring.coords) - origin
        axis.plot(*points.T, **style)


def build(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError(
            "Freeze the implementation before recording urban roof evidence"
        )
    output.mkdir(parents=True, exist_ok=True)
    rows, records = sources(), []
    figure = plt.figure(figsize=(24, 13.5))
    for index, row in enumerate(rows):
        p = attributes(row["properties"])
        choice = choose(row, p)
        result, envelope, top, _ = case(row, p, choice)
        base, _ = prism(row, p, 1)
        timings = {"hip_ms": [], "flat_ms": []}
        for _ in range(7):
            started = time.perf_counter()
            current, _, _ = roof_surface(row, choice)
            heightfield_envelope(current, p["minHeight"])
            timings["hip_ms"].append((time.perf_counter() - started) * 1000)
            started = time.perf_counter()
            prism(row, p, 1)
            timings["flat_ms"].append((time.perf_counter() - started) * 1000)
        x0, y0, x1, y1 = row["polygon"].bounds
        origin = np.array([(x0 + x1) / 2, (y0 + y1) / 2])
        a = figure.add_subplot(len(rows), 3, index * 3 + 1)
        vertices = top["vertices"]
        a.tripcolor(
            vertices[:, 0] - origin[0],
            vertices[:, 1] - origin[1],
            top["faces"],
            vertices[:, 2],
            shading="gouraud",
            cmap="cividis",
            vmin=choice.eave_m,
            vmax=p["buildingHeight"],
        )
        outline(a, row["polygon"], origin, color="black", linewidth=0.8)
        a.set_title(
            f"{row['id'].split('/')[0]} · {len(row['polygon'].interiors)} Höfe · Dachdeckung"
        )
        b = figure.add_subplot(len(rows), 3, index * 3 + 2)
        outline(b, row["polygon"], origin, color="#888888", linewidth=0.8)
        for fraction, colour in ((0.25, "#31819a"), (0.75, "#af6030")):
            section = mesh_section(
                envelope.mesh,
                envelope.vertex_roots,
                choice.eave_m + choice.rise_m * fraction,
            )
            for polygon in shapely.get_parts(section):
                outline(b, polygon, origin, color=colour, linewidth=1.4)
        b.set_title("Tatsächliche Körperschnitte · 25% / 75% Dachhöhe")
        for axis in (a, b):
            axis.set(aspect="equal", xlabel="Ost ab Zentrum [m]", ylabel="Nord [m]")
        c = figure.add_subplot(len(rows), 3, index * 3 + 3, projection="3d")
        triangles = envelope.mesh["vertices"][envelope.mesh["faces"]].copy()
        triangles[:, :, :2] -= origin
        normals = np.cross(
            triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        )
        normals /= np.linalg.norm(normals, axis=1, keepdims=True)
        shading = 0.35 + 0.65 * np.maximum(
            0, normals @ np.array([0.4, -0.5, 0.76811457])
        )
        colours = np.where(
            (envelope.surface_roles == 0)[:, None],
            [0.55, 0.64, 0.65],
            [0.83, 0.78, 0.69],
        )
        c.add_collection3d(
            Poly3DCollection(
                triangles, facecolors=colours * shading[:, None], edgecolors="none"
            )
        )
        c.set(
            xlim=(-(x1 - x0) / 2, (x1 - x0) / 2),
            ylim=(-(y1 - y0) / 2, (y1 - y0) / 2),
            zlim=(0, p["buildingHeight"]),
        )
        c.set_box_aspect((x1 - x0, y1 - y0, p["buildingHeight"]))
        c.view_init(elev=30, azim=-65)
        c.set_title(
            f"Geschlossene Hülle · Traufe {choice.eave_m:.2f} / Oberkante {p['buildingHeight']:.2f} m"
        )
        records.append(
            result
            | {
                "source_properties": row["properties"],
                "context_source_sha256": row["context"]["source_sha256"],
                "footprint_wkb": row["footprint_wkb"],
                "holes": len(row["polygon"].interiors),
                "choice": choice.__dict__,
                "timings_ms": timings,
                "median_hip_ms": float(np.median(timings["hip_ms"])),
                "median_flat_ms": float(np.median(timings["flat_ms"])),
                "hip_array_bytes": sum(v.nbytes for v in envelope.mesh.values()),
                "flat_array_bytes": sum(v.nbytes for v in base.values()),
                "flat_triangles": len(base["faces"]),
            }
        )
    figure.suptitle(
        "Echte Stadtgrundrisse · prozedurale Walmdächer innerhalb der Quellhöhe",
        fontsize=21,
    )
    figure.tight_layout(rect=(0, 0, 1, 0.96))
    figure.savefig(output / "urban-roof-sections.png", dpi=120)
    plt.close(figure)
    (output / "urban-roof-sections.json").write_text(
        json.dumps(
            proof
            | {
                "cases": records,
                "profile": [2880, 1620],
                "process_peak_rss_kib": resource.getrusage(
                    resource.RUSAGE_SELF
                ).ru_maxrss,
                "scope": "Incremental construction work for different exterior quality, not equal-quality algorithm race; independent solid checks outside timed loops. Seven warm sequential local runs under concurrent host load. Array bytes describe output buffers, not all allocations/copies; process RSS includes inputs/plots/verifiers. No native/device/frame admission.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    build(parser.parse_args().output)
