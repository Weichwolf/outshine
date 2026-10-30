"""Whole-source moving-pose controls; fine originals are an independent opponent."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from capture_box_envelope import box_envelope, conditionally_inside
from capture_box_reference import submitted_inputs
from capture_source_support import capture_support
from capture_support_reference import metadata_inputs, original_parts
from experiment_output import create_output
from field_pose_geometry import geometric_candidate, projection_bound
from field_view_projection import CapturePlane

ROOT = Path(__file__).resolve().parents[3]
DATA = ROOT / "test/outshine/data/render/building-field-composition"


def controls(lower, upper, camera):
    centre = (lower + upper) / 2
    direction = -np.asarray(camera["forward"])
    plane = CapturePlane(lower, upper, centre, direction)
    cases = []
    for name, distance, lateral, focal, raster in (
        ("near", 500, 0, 900, (1280, 720)),
        ("32km", 32000, 0, 900, (1280, 720)),
        ("64km", 64000, 0, 900, (1280, 720)),
        ("240km", 240000, 0, 900, (1280, 720)),
        ("lateral", 64000, 16000, 900, (1280, 720)),
        ("turn", 64000, 0, 900, (1280, 720)),
        ("double-raster", 32000, 0, 1800, (2560, 1440)),
    ):
        eye = centre + direction * distance + np.asarray(camera["right"]) * lateral
        forward = centre - eye
        forward /= np.linalg.norm(forward)
        right = np.cross(forward, [0, 1, 0])
        right /= np.linalg.norm(right)
        up = np.cross(right, forward)
        if name == "turn":
            angle = np.deg2rad(12)
            right, forward = (
                right * np.cos(angle) - forward * np.sin(angle),
                forward * np.cos(angle) + right * np.sin(angle),
            )
        matrix = np.array(
            [
                [2 * focal / raster[0], 0, 0, 0],
                [0, 2 * focal / raster[1], 0, 0],
                [0, 0, 0, 0.1],
                [0, 0, 1, 0],
            ],
            dtype=np.float32,
        )
        cases.append(
            {
                "name": name,
                "eye": eye.tolist(),
                "basis": np.array([right, up, forward]).tolist(),
                "projection": matrix.tolist(),
                "raster": raster,
            }
        )
    return plane, cases


def project(points, case):
    camera = (points - case["eye"]) @ np.asarray(case["basis"]).T
    clip = (
        np.column_stack((camera, np.ones(len(camera))))
        @ np.asarray(case["projection"]).T
    )
    return clip[:, :2] / clip[:, 3:] * (np.asarray(case["raster"]) / 2)


def plot(points, plane, cases, records, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    direction = np.asarray(plane.toward_eye)
    height = (points - plane.centre) @ direction / (direction @ direction)
    flat = points - height[:, None] * direction
    fig, axes = plt.subplots(3, 2, figsize=(11, 10), layout="constrained")
    for row, name in enumerate(("near", "32km", "lateral")):
        case = next(c for c in cases if c["name"] == name)
        record = next(r for r in records if r["name"] == name and r["plane"] == "flat")
        paired = [project(values, case) for values in (points, flat)]
        limits = np.concatenate(paired)
        for ax, xy, title in zip(
            axes[row],
            paired,
            ("original stored vertices", "capture-ray plane"),
            strict=True,
        ):
            ax.scatter(*xy.T, s=0.1, c="0.25", rasterized=True)
            ax.set_aspect("equal")
            ax.set_title(f"{name}: {title}")
            ax.set_xlabel("screen x relative to centre (px)")
            ax.set_ylabel("screen y relative to centre (px)")
            ax.grid(alpha=0.2)
            ax.set_xlim(limits[:, 0].min() - 0.1, limits[:, 0].max() + 0.1)
            ax.set_ylim(limits[:, 1].min() - 0.1, limits[:, 1].max() + 0.1)
        axes[row, 1].text(
            0.02,
            0.02,
            f"geometry L1 <= {record['bound']['screen_l1_upper_px']:.5g} px; candidate={record['candidate_at_0_25px']}",
            transform=axes[row, 1].transAxes,
        )
    fig.savefig(output / "whole-source-projection.png", dpi=140)
    plt.close(fig)


def run(args):
    create_output(args.output)
    manifest, arrays = metadata_inputs(DATA)
    lower, upper = arrays["source_least_m"].min(0), arrays["source_most_m"].max(0)
    capture = capture_support(lower[None], upper[None], manifest["capture_camera"])
    plane, cases = controls(lower, upper, manifest["capture_camera"])
    normals = {
        "flat": plane.toward_eye,
        "tilted-control": tuple(
            np.asarray(plane.toward_eye)
            - 0.2 * np.asarray(manifest["capture_camera"]["right"])
            + 0.1 * np.asarray(manifest["capture_camera"]["up"])
        ),
    }
    records = []
    for name, normal in normals.items():
        for case in cases:
            bound = projection_bound(
                plane,
                case["eye"],
                case["basis"],
                case["projection"],
                case["raster"],
                normal=normal,
            )
            records.append(
                {
                    "name": case["name"],
                    "plane": name,
                    "normal": normal,
                    "bound": bound,
                    "candidate_at_0_25px": bool(capture["inside"][0])
                    and geometric_candidate(bound, 0.25),
                }
            )
    # Fine data is read only after the metadata decisions, never by the kernel.
    parts = original_parts(manifest)
    points = np.concatenate([p["positions"] for p in parts]).astype(float)
    operand_manifest, submitted, operands = submitted_inputs()
    envelope = box_envelope(
        lower[None], upper[None], *operands, positions_normal_or_zero=True
    )
    for record in records:
        case = next(c for c in cases if c["name"] == record["name"])
        normal, direction = np.asarray(record["normal"]), np.asarray(plane.toward_eye)
        h = (points - plane.centre) @ normal / (normal @ direction)
        flat = points - h[:, None] * direction
        actual = abs(project(points, case) - project(flat, case))
        record["all_original_vertex_maximum_l1_px"] = float(actual.sum(1).max())
        if np.any(actual > record["bound"]["axis_upper_px"]):
            raise ValueError("Actual whole original exceeds its metadata envelope")
    model, view, shift = operands
    if not np.array_equal(model, np.eye(4, dtype=np.float32)):
        raise ValueError(
            "Fixed native carrier adapter requires its actual identity model"
        )
    native = projection_bound(plane, -shift.astype(float), np.eye(3), view, (128, 128))
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "source_archive_sha256": manifest["npz_sha256"],
        "native_operand_archive_sha256": operand_manifest["npz_sha256"],
        "source_ids": len(arrays["source_ids"]),
        "original_vertices": len(points),
        "whole_hull_bytes": lower.nbytes + upper.nbytes,
        "capture_inside": bool(capture["inside"][0]),
        "capture_expression_conditionally_inside": bool(
            conditionally_inside(envelope)[0]
        ),
        "plane": {
            "lower": list(plane.lower),
            "upper": list(plane.upper),
            "centre": list(plane.centre),
            "toward_eye": list(plane.toward_eye),
        },
        "cases": cases,
        "records": records,
        "native_orthographic_carrier_geometry": native,
        "scope": "Whole supported originals, geometric continuous-box bound only. 0.25 px is an explicit diagnostic budget, not product policy. Moving matrices and tilted-control plane are supplied finite controls, not new GPU observations or fitted native fields. Source 3825/3827 and Cell33 opacity remain unqualified. No colour, coverage, visibility, main/shadowdepth or World replacement certificate.",
        "loaded_fine_after_admission_bytes": points.nbytes
        + submitted["vertex_positions_f32"].nbytes,
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    plot(points, plane, cases, records, args.output)
    (args.output / "plot.sha256").write_text(
        hashlib.sha256(
            (args.output / "whole-source-projection.png").read_bytes()
        ).hexdigest()
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
