"""Fresh unexaggerated validation views, kept outside the four-image public showcase."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from endpoint_grade_experiment import OUTPUT, SURFACE
from ground_surface_showcase import clipped
from outshine_ref.contracts import Mesh
from outshine_ref.transport import (
    close_surface,
    plan_endpoint_transition,
    plan_vertical_transition,
    sample_vertical_transition,
)
from surface_solid_showcase import write_case
from vertical_transition_experiment import ribbon
from whole_tile_showcase import tile_region

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
RENDER = OUTPUT / "views"


def selected_cases():
    report = json.loads((OUTPUT / "envelope-0/report.json").read_text())
    selected = max(
        (r for r in report["cases"] if r["valid"]),
        key=lambda r: max(abs(v) for v in r["end_grades"]),
    )
    control = {
        "source_id": "synthetic-affine-contacts",
        "xy": [[0, 0], [120, 0]],
        "direction": [1, 0],
        "length_m": 120,
        "recipe_width_m": 8,
        "end_heights_m": [0, 2],
        "end_grades": [0.06, -0.02],
        "grade_bound": 0.064,
        "longitudinal_grade": 0.08,
        "crossfall": 0.015,
        "scope": "Synthetic straight full-width affine end planes, original geometry scale.",
    }
    return [("control", control), ("Vienna-soil-contact", selected)]


def build_profile_views(configurations):
    for name, row in selected_cases():
        length = row["length_m"]
        direction = np.asarray(row["direction"])
        start = np.asarray(row["xy"][0])
        z = row["end_heights_m"]
        g0, g1 = row["end_grades"]
        pad = min(20.0, length / 5)
        for method in ("flat-diagnostic", "hermite", "envelope"):
            if method == "flat-diagnostic":
                profile, note = plan_vertical_transition(
                    *z, length, grade=row["grade_bound"]
                )
            else:
                profile, note = plan_endpoint_transition(
                    *z, length, g0, g1, grade=row["grade_bound"], method=method
                )
            if profile is None:
                raise ValueError(f"inspection profile failed: {note}")
            table, sampling = sample_vertical_transition(profile, height_error_m=0.002)
            table = np.vstack(
                ([-pad, z[0] - g0 * pad], table, [length + pad, z[1] + g1 * pad])
            )
            top = ribbon(
                start, direction, table, row["recipe_width_m"], row["crossfall"]
            )
            body = close_surface(top, 0.35)
            materials = np.full(len(body.triangles), 2)
            centers = top.vertices[top.triangles, :2].mean(axis=1)
            stations = (centers - start) @ direction
            materials[: len(top.triangles)] = np.where(
                (stations < 0) | (stations > length), 1, 0
            )
            middle = np.r_[start + direction * length / 2, sum(z) / 2]
            transverse = np.array([-direction[1], direction[0], 0.0])
            view = {
                "eye": (middle + transverse * length + [0, 0, length * 0.15]).tolist(),
                "target": middle.tolist(),
                "span_m": (length + 2 * pad) * 1.1,
                "profile": [1920, 1080],
            }
            write_case(
                name + "-" + method,
                body,
                materials,
                {
                    "source": row,
                    "profile": note,
                    "sampling": sampling,
                    "diagnostic": method == "flat-diagnostic",
                    "height_exaggeration": 1,
                    "scope": "Full independent ribbon with explicitly chosen affine exterior pads. Flat comparator intentionally has tangent jumps. Actual Vienna cases use exterior-soil planes, not assembled road-junction ports; pads display these planes beyond the contacts. Finite faces C0.",
                },
                configurations,
                view=view,
                output_dir=RENDER,
            )


def build_whole_tile(configurations):
    region, source = tile_region("Vienna")
    with np.load(SURFACE) as data:
        top = Mesh(data["vertices"], data["triangles"])
        materials = np.where(
            np.arange(len(top.triangles)) < int(data["road_face_count"]), 1, 5
        )
    top, materials, weld = clipped(top, materials, region, max_faces=262144)
    body = close_surface(top, 5)
    slots = np.full(len(body.triangles), 2)
    slots[: len(top.triangles)] = materials
    write_case(
        "Vienna-complete-parent-tile",
        body,
        slots,
        {
            "source": source,
            "selected_surface_sha256": hashlib.sha256(SURFACE.read_bytes()).hexdigest(),
            "full_tile_area_m2": float(region.area),
            "clipping_weld_m": weld,
            "height_exaggeration": 1,
            "scope": "Entire original selected shared road/soil tile, unchanged contextual height mesh. New independent candidate ribbons are not composed into it. This image locates the real input scope; it does not prove a new C1 network or combined terrain.",
        },
        configurations,
        output_dir=RENDER,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-only", action="store_true")
    args = parser.parse_args()
    RENDER.mkdir(parents=True, exist_ok=True)
    configurations = []
    build_profile_views(configurations)
    build_whole_tile(configurations)
    scenes = RENDER / "scenes.json"
    scenes.write_text(json.dumps(configurations, indent=2) + "\n")
    if args.build_only:
        return
    subprocess.run(
        [
            "blender",
            "--background",
            "--factory-startup",
            "--threads",
            "2",
            "--python-exit-code",
            "1",
            "--python",
            str(HERE / "render_transport_solid.py"),
            "--",
            str(scenes),
        ],
        cwd=ROOT,
        check=True,
        timeout=180,
    )
    proof = {
        row["id"]: {
            "image_sha256": hashlib.sha256(
                Path(row["output"]).read_bytes()
            ).hexdigest(),
            "mesh_sha256": row["mesh_sha256"],
        }
        for row in configurations
    }
    (RENDER / "origin.json").write_text(
        json.dumps(
            {
                "commit": subprocess.check_output(
                    ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
                ).strip(),
                "working_tree_dirty": bool(
                    subprocess.check_output(
                        [
                            "git",
                            "status",
                            "--porcelain",
                            "--",
                            "lib",
                            "test/experiments/transport",
                        ],
                        cwd=ROOT,
                        text=True,
                    ).strip()
                ),
                "records": proof,
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
