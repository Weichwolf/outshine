"""Actual source-owned patch placement bounds, separate from omitted coverage."""

import argparse
import hashlib
import json
import platform
import resource
import sys
import tempfile
import time
from fractions import Fraction
from pathlib import Path

import numpy as np
from check_building_capture_inputs import check, load
from patch_projection_bound import (
    WEIGHT_SCALE,
    directed_correspondence,
    projection_bound,
)


def triangles(chunk):
    parts, bindings = [], []
    for stream, name in enumerate(("walls", "roofs")):
        data = chunk[name]
        if not data["triangles"]:
            continue
        parts.append(
            np.asarray(data["positions"], np.float32)[
                np.asarray(data["triangles"], np.int64)
            ]
        )
        bindings.extend(
            (stream, triangle) for triangle in range(len(data["triangles"]))
        )
    return np.concatenate(parts), np.asarray(bindings, np.int32)


def control(source, target, proof, eye, radius, focal, width, height):
    """Dense independent NumPy interpolation/projection, never the certificate."""
    p = np.einsum(
        "nij,njk->nik",
        proof["source_weights"] / proof["source_scale"],
        source[proof["source_triangle"]].astype(float),
    )
    q = np.einsum(
        "nij,njk->nik",
        proof["target_weights"] / WEIGHT_SCALE,
        target[proof["target_triangle"]].astype(float),
    )
    rng = np.random.default_rng(867)
    samples = np.r_[np.eye(3), rng.dirichlet([1, 1, 1], 13)]
    original = np.einsum("ij,njk->nik", samples, p).reshape(-1, 3)
    mapped = np.einsum("ij,njk->nik", samples, q).reshape(-1, 3)
    spatial = np.linalg.norm(original - mapped, axis=1).max(initial=0)
    if spatial**2 > float(proof["squared_m"]) + 1e-8:
        raise ValueError("independent surface control exceeds correspondence bound")
    directions = np.array(
        [
            [x, y, z]
            for x in (-1, 0, 1)
            for y in (-1, 0, 1)
            for z in (-1, 0, 1)
            if x or y or z
        ],
        float,
    )
    directions /= np.linalg.norm(directions, axis=1)[:, None]
    eyes = np.r_[eye[None], eye + radius * directions]
    maximum, tested = 0.0, 0
    for position in eyes:
        forward = original.mean(0) - position
        forward /= np.linalg.norm(forward)
        hint = [0, 0, 1] if abs(forward[2]) < 0.9 else [0, 1, 0]
        right = np.cross(forward, hint)
        right /= np.linalg.norm(right)
        basis = np.stack((right, np.cross(right, forward), forward))
        for side in (-1, 0, 1):
            yaw = side * 0.9 * np.arctan(width / (2 * focal))
            pitch = side * 0.9 * np.arctan(height / (2 * focal))
            turn = np.array(
                [
                    [np.cos(yaw), 0, -np.sin(yaw)],
                    [0, 1, 0],
                    [np.sin(yaw), 0, np.cos(yaw)],
                ]
            )
            tilt = np.array(
                [
                    [1, 0, 0],
                    [0, np.cos(pitch), -np.sin(pitch)],
                    [0, np.sin(pitch), np.cos(pitch)],
                ]
            )
            camera = tilt @ turn @ basis
            a, b = (points - position for points in (original, mapped))
            a, b = a @ camera.T, b @ camera.T
            valid = (a[:, 2] > 0) & (b[:, 2] > 0)
            inside = (abs(a[:, :2]) * focal <= a[:, 2:] * [width / 2, height / 2]).all(
                1
            )
            valid &= inside
            error = focal * np.linalg.norm(
                a[valid, :2] / a[valid, 2:] - b[valid, :2] / b[valid, 2:], axis=1
            )
            maximum = max(maximum, error.max(initial=0))
            tested += len(error)
    return {
        "sampled_spatial_max_m": spatial,
        "sampled_projection_max_px": maximum,
        "projected_pairs": tested,
        "eye_samples": len(eyes),
        "camera_samples_per_eye": 3,
    }


def measure(source, target, eye, radius, focal, allowed, output):
    best, attempts = None, []
    for depth in (0, 1, 2):
        began = time.perf_counter()
        proof = directed_correspondence(source, target, depth=depth)
        bound = projection_bound(
            proof["squared_m"], source, target, eye, radius, focal, 1280, 720, allowed
        )
        attempts.append(
            {
                "depth": depth,
                "pairs": proof["pairs"],
                "exact_vertex_checks": proof.get("exact_vertex_checks", 0),
                "proposal_operand_extent_bytes": proof.get(
                    "proposal_operand_extent_bytes", 0
                ),
                "seconds": time.perf_counter() - began,
                "upper_px": bound["upper_px"],
                "reason": proof.get("reason", bound.get("reason")),
            }
        )
        if bound["upper_px"] is not None and (
            best is None or bound["upper_px"] < best[0]["upper_px"]
        ):
            best = bound, proof, depth
        if bound["within"]:
            break
    if best is None:
        return {"upper_px": None, "within": False, "attempts": attempts}
    bound, proof, depth = best
    evidence = control(source, target, proof, eye, radius, focal, 1280, 720)
    if evidence["sampled_projection_max_px"] > float(bound["upper_px"]) + 1e-8:
        raise ValueError("independent projection control exceeds rational bound")
    arrays = {
        name: value for name, value in proof.items() if isinstance(value, np.ndarray)
    }
    arrays["source_scale"] = np.asarray(proof["source_scale"])
    arrays["target_scale"] = np.asarray(WEIGHT_SCALE)
    np.savez_compressed(output, **arrays)
    return {
        **bound,
        "squared_m": proof["squared_m"],
        "depth": depth,
        "attempts": attempts,
        "controls": evidence,
        "retained_witness_array_bytes": sum(a.nbytes for a in arrays.values()),
    }


def picture(records, geometry, eye, focal, radius, allowed, output):
    import matplotlib.pyplot as plt

    figure, axes = plt.subplots(2, 2, figsize=(14, 7))
    for ax, name, title in zip(
        axes[0],
        ("patch_to_full", "full_to_patch"),
        (
            "Patch placement toward full face",
            "Whole face toward cropped patch: coverage control",
        ),
        strict=True,
    ):
        upper = [
            float(r[name]["upper_px"]) if r[name]["upper_px"] is not None else np.nan
            for r in records
        ]
        sampled = [
            r[name].get("controls", {}).get("sampled_projection_max_px", np.nan)
            for r in records
        ]
        ax.bar(np.arange(len(records)), upper, label="rational upper bound")
        ax.scatter(
            np.arange(len(records)),
            sampled,
            color="black",
            s=10,
            label="independent samples",
        )
        ax.axhline(allowed, color="red", label=f"{allowed:g} px geometric requirement")
        ax.set_xticks(
            np.arange(len(records)),
            [f"{r['slot']}/{r['face']}" for r in records],
            rotation=90,
        )
        ax.set(
            title=title,
            ylabel="pixel distance",
            xlabel="original source slot / FaceAt index",
        )
        ax.legend(fontsize=7)
    worst = max(
        range(len(records)),
        key=lambda i: float(records[i]["patch_to_full"]["upper_px"] or 0),
    )
    source, candidate = geometry[worst]
    centre = source.reshape(-1, 3).mean(0)
    forward = centre - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, [0, 0, 1])
    right /= np.linalg.norm(right)
    basis = np.stack((right, np.cross(right, forward), forward))
    for ax, move in zip(axes[1], (0, radius), strict=True):
        for triangles_, colour, label in (
            (source, "blue", "full original face"),
            (candidate, "red", "actual native patch"),
        ):
            local = (triangles_ - eye - move * right) @ basis.T
            points = focal * local[:, :, :2] / local[:, :, 2:]
            for index, triangle in enumerate(points):
                ring = np.r_[triangle, triangle[:1]]
                ax.plot(
                    ring[:, 0],
                    ring[:, 1],
                    color=colour,
                    label=label if index == 0 else None,
                )
        ax.set_aspect("equal")
        ax.set(
            title=f"slot {records[worst]['slot']}, face {records[worst]['face']}; eye +{move}m",
            xlabel="offset from screen centre (px)",
            ylabel="offset from screen centre (px)",
        )
        ax.legend(fontsize=7)
    figure.suptitle(
        "Actual 1280×720 / native focal; geometry only, no visibility or material admission"
    )
    figure.tight_layout()
    figure.savefig(output)
    plt.close(figure)


def encode(value):
    if isinstance(value, Fraction):
        return {
            "numerator": value.numerator,
            "denominator": value.denominator,
            "display": float(value),
        }
    if isinstance(value, np.generic):
        return value.item()
    raise TypeError(type(value).__name__)


def run(args):
    began = time.perf_counter()
    document = load(args.input)
    check(document)
    calls = [c for r in document["results"] for c in r["projection_calls"]]
    if len(calls) != 1:
        raise ValueError("one original native projection required")
    call = calls[0]
    capture = call["capture"]
    eye, focal, allowed = (
        np.asarray(capture["eye_offset_ecef"], float),
        capture["focal_px"],
        capture["error_px"],
    )
    args.output.mkdir(parents=True, exist_ok=False)
    records, geometry = [], []
    original_sources = {row["index"]: row for row in document["sources"]}
    for source in capture["sources"]:
        if source["emission"] != 2:
            continue
        for patch_index, chunk in enumerate(source["captured"]):
            face = chunk["face_index"]
            a, a_bindings = triangles(source["reference"][face])
            b, b_bindings = triangles(chunk)
            prefix = f"s{source['slot']}-f{face}-p{patch_index}"
            np.savez_compressed(
                args.output / f"{prefix}-bindings.npz",
                full=a_bindings,
                patch=b_bindings,
            )
            row = {
                "slot": source["slot"],
                "member": call["members"][source["slot"]],
                "original": original_sources.get(
                    call["members"][source["slot"]]["source"]
                ),
                "part": chunk["part"],
                "face": face,
                "side": chunk["side"],
                "patch_index": patch_index,
                "gable": source["patches"][patch_index]["gable"],
                "full_patch_triangles": [len(a), len(b)],
            }
            for name, first, second in (
                ("patch_to_full", b, a),
                ("full_to_patch", a, b),
            ):
                row[name] = measure(
                    first,
                    second,
                    eye,
                    args.radius_m,
                    focal,
                    allowed,
                    args.output / f"{prefix}-{name}.npz",
                )
            records.append(row)
            geometry.append((a, b))
    if not records:
        raise ValueError("no actual native patches")
    picture(
        records,
        geometry,
        eye,
        focal,
        args.radius_m,
        allowed,
        args.output / "patch-projection.png",
    )
    report = {
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "radius_m": args.radius_m,
        "width_height": [1280, 720],
        "focal_px": focal,
        "allowed_error_px": allowed,
        "platform": platform.platform(),
        "native_provenance": document["provenance"],
        "triangle_binding_columns": ["stream", "original_chunk_triangle"],
        "records": records,
        "seconds": time.perf_counter() - began,
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1024 if sys.platform.startswith("linux") else 1)
        if sys.platform.startswith("linux") or sys.platform == "darwin"
        else None,
        "scope": "Directed geometric support bounds only. Full native faces include intentionally occluded/cropped areas; reverse failure does not by itself violate the native error contract. No visibility omissions, clipping, depth order, PBR/normal/UV/material, shadow, native arithmetic/device or whole-capture reuse admission. Proposal byte extents count logical operands, not allocations/copies; Fraction and library scratch allocations are unmeasured.",
    }
    (args.output / "report.json").write_text(
        json.dumps(report, default=encode, indent=2) + "\n"
    )
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--radius-m", type=float, default=6)
    args = parser.parse_args()
    if (
        not np.isfinite(args.radius_m)
        or args.radius_m < 0
        or not args.output.resolve().is_relative_to(
            Path(tempfile.gettempdir()).resolve()
        )
    ):
        parser.error("finite nonnegative radius and fresh system-temp output required")
    result = run(args)
    print(
        json.dumps(
            {
                "patches": len(result["records"]),
                "directed_admitted": {
                    name: sum(r[name]["within"] for r in result["records"])
                    for name in ("patch_to_full", "full_to_patch")
                },
                "seconds": result["seconds"],
            }
        )
    )
