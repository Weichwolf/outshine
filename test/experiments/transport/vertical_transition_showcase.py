"""Flat ports before/after C1 station profiles; synthetic and saved Vienna axes."""

import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from outshine_ref.transport import (
    close_surface,
    plan_vertical_transition,
    sample_vertical_transition,
)
from surface_solid_showcase import write_case
from vertical_transition_experiment import ribbon

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUTPUT = ROOT / "build/experiments/transport/vertical-profiles"


def build_cases():
    report = json.loads((OUTPUT / "comparison.json").read_text())
    selected = report["inspection"]
    if selected is None:
        raise ValueError("no saved real profile available for the inspection")
    configurations = []
    for case, start, direction, z, length, width, grade in (
        (
            "ramp-control",
            np.array([0.0, 0.0]),
            np.array([1.0, 0.0]),
            [0.0, 6.0],
            160.0,
            8.0,
            0.08,
        ),
        (
            "Vienna-profile",
            np.asarray(selected["xy"][0]),
            (np.asarray(selected["xy"][1]) - selected["xy"][0]) / selected["length_m"],
            selected["target_heights_m"],
            selected["length_m"],
            selected["width_m"],
            selected["grade"],
        ),
    ):
        began = len(configurations)
        for method in ("linear", "parabolic", "cubic"):
            if method == "linear":
                table = np.array([[0.0, z[0]], [length, z[1]]])
                metadata = {"flat_port_grade_jump": abs(z[1] - z[0]) / length}
            else:
                profile, metadata = plan_vertical_transition(
                    *z, length, grade=grade, method=method
                )
                if profile is None:
                    raise ValueError("comparison contains infeasible real method")
                table, sampling = sample_vertical_transition(
                    profile, height_error_m=0.002
                )
                metadata.update(sampling=sampling, flat_port_grade_jump=0.0)
            pad = min(20.0, length / 8)
            table = np.vstack(([-pad, z[0]], table, [length + pad, z[1]]))
            full = ribbon(start, direction, table, width)
            np.savez_compressed(
                OUTPUT / f"{case}-{method}-full.npz",
                vertices=full.vertices,
                triangles=full.triangles,
            )
            section_end = min(30.0, length)
            detail = np.vstack(
                (
                    table[table[:, 0] < section_end],
                    [section_end, np.interp(section_end, table[:, 0], table[:, 1])],
                )
            )
            top = ribbon(start, direction, detail, width)
            body = close_surface(top, 0.35)
            record = dict(
                metadata,
                source=selected if case == "Vienna-profile" else None,
                displayed_station_interval_m=[-pad, section_end],
                full_profile_length_m=length,
                full_mesh_vertices=len(full.vertices),
                full_mesh_triangles=len(full.triangles),
                scope="First-contact 3D section of the exact finite full ribbon, cropped by linear interpolation of its chord mesh. Actual crossfall0.015, flat pads and 0.35m display closure; no height exaggeration. Real DEM end targets, explicit flat-end recipe policy. C1 analytic plan; finite faces retain bounded grade jumps. No full junction/terrain/bridge or runtime proof.",
            )
            colours = np.full(len(body.triangles), 2, dtype=int)
            colours[: len(top.triangles)] = 0
            pad_faces = np.all(
                top.vertices[top.triangles, :2] @ direction <= start @ direction + 1e-9,
                axis=1,
            )
            colours[: len(top.triangles)][pad_faces] = 1
            shown_length = section_end + pad
            middle = np.r_[
                start + direction * (section_end - pad) / 2, (z[0] + detail[-1, 1]) / 2
            ]
            normal = np.array([-direction[1], direction[0], 0.0])
            view = {
                "eye": (
                    middle + normal * shown_length + [0.0, 0.0, shown_length * 0.25]
                ).tolist(),
                "target": middle.tolist(),
                "span_m": shown_length * 1.12,
                "profile": [1920, 1080],
            }
            write_case(
                case + "-" + method,
                body,
                colours,
                record,
                configurations,
                view=view,
                output_dir=OUTPUT,
            )
            for key in ("eye", "target", "span_m", "profile"):
                configurations[-1][key] = configurations[began][key]
    return configurations


def main():
    configurations = build_cases()
    scene = OUTPUT / "scenes.json"
    scene.write_text(json.dumps(configurations, indent=2) + "\n")
    subprocess.run(
        [
            "blender",
            "--background",
            "--factory-startup",
            "--python-exit-code",
            "1",
            "--python",
            str(HERE / "render_transport_solid.py"),
            "--",
            str(scene),
        ],
        cwd=ROOT,
        check=True,
    )
    records = {}
    for row in configurations:
        stem = OUTPUT / (row["id"] + "-solid")
        records[stem.name] = {
            "image_sha256": hashlib.sha256(
                stem.with_suffix(".png").read_bytes()
            ).hexdigest(),
            "report_sha256": hashlib.sha256(
                stem.with_suffix(".json").read_bytes()
            ).hexdigest(),
        }
    (OUTPUT / "origin.json").write_text(
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
                    )
                ),
                "records": records,
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
