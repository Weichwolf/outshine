"""Compare native box queries; this is not a PBR/frame/World budget admission."""

import argparse
import json
import os
import platform
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from bench_box_field import camera, image, load, rays
from box_field_gpu import ROOT, build, encode, run, sha
from box_field_query import BoxField
from PIL import Image
from triangle_image_reference import box_grid_source, intersect_source, prepare_source


def packet(origins, directions):
    result = np.zeros((len(origins), 2, 4), "<f4")
    result[:, 0, :3], result[:, 1, :3] = origins, directions
    return result


def compare(depth, normal, expected, expected_normal, scale):
    coverage = np.isfinite(depth) != np.isfinite(expected)
    finite = np.isfinite(depth) & np.isfinite(expected)
    error = np.abs(depth[finite] - expected[finite])
    normal_errors = np.any(normal[finite] != expected_normal[finite], axis=1)
    # Diagnostic Float32 allowance, not a derived rendering/LOD error bound.
    tolerance = (
        32 * np.finfo(np.float32).eps * np.maximum(scale, np.abs(expected[finite]))
    )
    report = {
        "rays": len(depth),
        "coverage_mismatches": int(coverage.sum()),
        "normal_mismatches": int(normal_errors.sum()),
        "max_depth_error_m": float(error.max(initial=0)),
        "depth_outside_diagnostic_tolerance": int(np.count_nonzero(error > tolerance)),
    }
    if coverage.any() or normal_errors.any() or np.any(error > tolerance):
        raise ValueError(f"query disagrees with reference: {report}")
    return report


def controls(native, output, validation):
    rng = np.random.default_rng(1127)
    cases = []
    for shape, fill in [((7, 13), 0.75), ((1, 200), 1.0), ((11, 3), 0.25)]:
        heights = rng.choice([0.0, 6.0, 12.0, 48.0], shape).astype(np.float32)
        if shape[0] == 1:
            heights.fill(48)
        field = BoxField(heights, 3.0, fill)
        angle = np.arange(256) * (2 * np.pi / 256)
        radius = max(shape) * 3.0
        origins = np.column_stack(
            (
                shape[1] * 1.5 + np.cos(angle) * radius,
                shape[0] * 1.5 + np.sin(angle) * radius,
                np.full(256, 80.0),
            )
        )
        target = rng.uniform([0, 0, 0], [shape[1] * 3, shape[0] * 3, 1], (256, 3))
        direction = target - origins
        direction /= np.linalg.norm(direction, axis=1)[:, None]
        cases.append((field, packet(origins, direction), True))
    field = BoxField([[6, 0], [0, 12]], 2.0, 0.75)
    origins = [[1, 1, 10], [1, -2, 2], [-2, 1, 2], [1, 1, 2], [4, 4, 10], [1, 1, -2]]
    directions = [[0, 0, -1], [0, 1, 0], [1, 0, 0], [0, 0, 1], [0, 0, -1], [0, 0, 1]]
    cases += [
        (field, packet(origins, directions), False),
        (BoxField(np.zeros((3, 5)), 2.0, 0.75), packet(origins, directions), False),
    ]
    corridor = BoxField(np.full((127, 33), 48.0), 2.0, 0.75)
    cases.append(
        (corridor, packet([[16, -2, 8]] * 2, [[0, 1, 0], [1e-5, 1, 0]]), False)
    )
    reports = []
    for index, (field, probes, triangles) in enumerate(cases):
        source = output / f"control-{index}.input"
        encoded, _ = encode(field, probes)
        source.write_bytes(encoded)
        origins, directions = (
            probes[:, 0, :3].astype(float),
            probes[:, 1, :3].astype(float),
        )
        expected, normals, _ = field.trace_direct(origins, directions)
        if triangles:
            values = field.mips[0][:-1, :-1]
            heights = field.palette[values] if field.palette is not None else values
            mesh = box_grid_source(heights, field.pitch, field.fill)
            prepared = prepare_source(mesh)
            reference, faces = intersect_source(
                mesh, origins, directions, prepared=prepared
            )
            reference_normal = np.zeros_like(normals)
            keep = np.isfinite(reference)
            selected = prepared["normal"][faces[keep]]
            reference_normal[keep] = (
                selected / np.linalg.norm(selected, axis=1)[:, None]
            )
            compare(
                expected, normals, reference, reference_normal, max(origins.max(), 1)
            )
        for method in ("direct", "hierarchy"):
            prefix = output / f"control-{index}-{method}"
            depth, normal, record = run(
                native, output, source, prefix, method, validation=validation
            )
            record["oracle"] = compare(
                depth, normal, expected, normals, max(origins.max(), 1)
            )
            reports.append(record)
            Path(f"{prefix}.hits").unlink()
        source.unlink()
    return reports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native", type=Path, default=ROOT)
    parser.add_argument(
        "--glslang", default=os.environ.get("GLSLANG", "glslangValidator")
    )
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--altitude", type=float, default=100)
    parser.add_argument("--oracle-rays", type=int, default=4096)
    parser.add_argument("--validation", action="store_true")
    args = parser.parse_args()
    output, native = args.output.resolve(), args.native.resolve()
    if not any(
        output.is_relative_to(p.resolve())
        for p in (Path(tempfile.gettempdir()), Path("/tmp"))
    ):
        parser.error("output must be in the system temp directory")
    count = args.width * args.height
    if min(args.width, args.height) <= 0 or count > 2097152 or args.oracle_rays < 1:
        parser.error("positive bounded frame and oracle sample required")
    if not np.isfinite(args.altitude) or args.altitude <= 0:
        parser.error("camera altitude must be finite and positive")
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("output must be empty")
    receipt = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=native, text=True
        ).strip(),
        "platform": platform.platform(),
        "scope": "query only; no native PBR/shadow/frame/World admission",
        "input": str(args.input.resolve()),
        "input_sha256": sha(args.input),
        "frame": [args.width, args.height],
        "altitude_m": args.altitude,
        "build": build(native, output, args.glslang),
    }
    receipt["controls"] = controls(native, output, args.validation)
    heights, pitch, fill = load(args.input)
    field = BoxField(heights, float(np.float32(pitch)), float(np.float32(fill)))
    receipt["shape"], receipt["occupied"] = (
        list(heights.shape),
        int(np.count_nonzero(heights)),
    )
    basis = np.zeros((4, 4), "<f4")
    basis[:, :3] = camera(field, args.altitude)
    source = output / "camera.input"
    encoded, receipt["requested_bytes"] = encode(
        field, basis, frame=(args.width, args.height)
    )
    source.write_bytes(encoded)
    receipt["query_input_sha256"] = sha(source)
    origins, directions = rays(field, args.width, args.height, args.altitude, 1)
    sample = np.linspace(0, count - 1, min(count, args.oracle_rays), dtype=int)
    probes = packet(origins[sample], directions[sample])
    expected, normals, _ = field.trace_direct(
        probes[:, 0, :3].astype(float), probes[:, 1, :3].astype(float)
    )
    scale = max(
        float(np.abs(origins).max()),
        field.columns * field.pitch,
        field.rows * field.pitch,
        1,
    )
    receipt["runs"] = []
    first = None
    for index, method in enumerate(("direct", "hierarchy", "hierarchy", "direct")):
        prefix = output / f"{index}-{method}"
        depth, normal, record = run(
            native, output, source, prefix, method, validation=args.validation
        )
        record["oracle"] = compare(
            depth[sample], normal[sample], expected, normals, scale
        )
        if first is None:
            first = depth.copy(), normal.copy()
        else:
            record["all_rays_against_first"] = compare(depth, normal, *first, scale)
        Image.fromarray(
            image(depth, normal, origins, directions, args.width, args.height, 1)
        ).save(f"{prefix}.png")
        receipt["runs"].append(record)
        (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
        Path(f"{prefix}.hits").unlink()
        print(method, json.dumps(record["gpu_submission_ms"]), flush=True)
    receipt["work_runs"] = []
    for method in ("direct-work", "hierarchy-work"):
        prefix = output / method
        depth, normal, record = run(
            native, output, source, prefix, method, validation=args.validation
        )
        record["all_rays_against_first"] = compare(depth, normal, *first, scale)
        record["timing_scope"] = "instrumented; excluded from normal timing comparison"
        receipt["work_runs"].append(record)
        Path(f"{prefix}.hits").unlink()
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    source.unlink()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
