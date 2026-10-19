"""Original native Fine geometry: bounded proposals and exact region omissions."""

import argparse
import hashlib
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from capture_region_candidates import propose
from capture_region_visibility import covered_from_ball
from compare_depth_cluster import face_sources
from compare_native_cell_views import low_eye_view
from current_visibility import project, raster
from source_far_assets import load_source


def solve(triangles, eye, basis, radius):
    began = time.perf_counter()
    blockers, report = propose(triangles, eye, basis)
    report["proposal_ms"] = (time.perf_counter() - began) * 1000
    point = np.zeros(len(triangles), bool)
    region = point.copy()
    plane_tests, calls = 0, 0
    began = time.perf_counter()
    for target in np.flatnonzero(blockers >= 0):
        blocker = blockers[target]
        for distance, admitted in ((0, point), (radius, region)):
            if distance and not point[target]:
                continue
            result = covered_from_ball(
                triangles[blocker],
                triangles[target],
                eye,
                distance,
                opaque=True,
                cull_backfaces=True,
            )
            calls += 1
            plane_tests += result["exact_plane_tests"]
            admitted[target] = result["covered"]
    report.update(
        predicate_ms=(time.perf_counter() - began) * 1000,
        predicate_calls=calls,
        exact_plane_tests=plane_tests,
        point_omitted=int(point.sum()),
        region_omitted=int(region.sum()),
        retained_index_bytes_point_region=[
            int(12 * (~m).sum()) for m in (point, region)
        ],
    )
    return blockers, point, region, report


def compare(source, masks, view):
    projected, z = project(source["positions"].astype(float), view)
    frames = [
        raster(projected, z, source["indices"][~mask], *view[3:]) for mask in masks
    ]
    full = frames[0]
    records = []
    for frame in frames:
        both = (full > 0) & (frame > 0)
        changed = both & (full != frame)
        records.append(
            {
                "full_covered_pixels": int((full > 0).sum()),
                "lost_pixels": int(((full > 0) & (frame == 0)).sum()),
                "extra_pixels": int(((full == 0) & (frame > 0)).sum()),
                "different_depth_pixels": int(changed.sum()),
                "maximum_inverse_depth_difference": float(abs(full - frame).max()),
            }
        )
    if not records[0]["full_covered_pixels"]:
        raise ValueError("empty view does not prove coverage")
    if any(
        records[-1][k]
        for k in ("lost_pixels", "extra_pixels", "different_depth_pixels")
    ):
        raise ValueError(f"region subset differs from opaque full reference: {records}")
    return frames, records


def verify_segments(triangles, blockers, region, eye, radius):
    """Independent segment/triangle linear systems at sampled ball-boundary eyes."""
    selected = np.flatnonzero(region)
    rng = np.random.default_rng(844)
    tested = 0
    for begin in range(0, len(selected), 64):
        targets = np.repeat(selected[begin : begin + 64], 16)
        direction = rng.normal(size=(len(targets), 3))
        origins = eye + radius * direction / np.linalg.norm(direction, axis=1)[:, None]
        weights = rng.dirichlet([1, 1, 1], len(targets))
        points = np.einsum("ij,ijk->ik", weights, triangles[targets])
        a, b, c = triangles[blockers[targets]].transpose(1, 0, 2)
        systems = np.stack((b - a, c - a, origins - points), axis=2)
        uv_t = np.linalg.solve(systems, (origins - a)[..., None])[..., 0]
        valid = (uv_t[:, :2] >= 0).all(1) & (uv_t[:, :2].sum(1) <= 1)
        valid &= (uv_t[:, 2] > 0) & (uv_t[:, 2] < 1)
        if not valid.all():
            raise ValueError(
                "independent segment reader contradicts region certificate"
            )
        tested += len(targets)
    return tested


def panel(rows, output):
    from PIL import Image, ImageDraw

    image = Image.new("RGB", (3 * 640, len(rows) * 390), "white")
    draw = ImageDraw.Draw(image)
    for row, (name, frames, records) in enumerate(rows):
        full = frames[0]
        yy, xx = np.nonzero(full > 0)
        crop = (int(xx.min()), int(yy.min()), int(xx.max() + 1), int(yy.max() + 1))
        lo, hi = np.percentile(full[full > 0], [1, 99])
        for column, (frame, record) in enumerate(zip(frames, records, strict=True)):
            grey = np.clip((frame - lo) / max(hi - lo, np.finfo(float).eps), 0, 1)
            rgb = np.repeat((60 + grey[..., None] * 170).astype(np.uint8), 3, axis=2)
            rgb[frame == 0] = [16, 18, 22]
            rgb[(full > 0) & (frame == 0)] = [255, 30, 30]
            rgb[(full > 0) & (frame > 0) & (full != frame)] = [255, 220, 20]
            thumb = Image.fromarray(rgb).crop(crop)
            scale = min(640 / thumb.width, 360 / thumb.height)
            thumb = thumb.resize(
                (
                    max(1, round(thumb.width * scale)),
                    max(1, round(thumb.height * scale)),
                ),
                Image.Resampling.NEAREST,
            )
            image.paste(thumb, (column * 640, row * 390 + 30))
            draw.text(
                (column * 640 + 5, row * 390 + 5),
                f"{name} / {('Full', 'Point', 'Region')[column]} / lost {record['lost_pixels']} / depth {record['different_depth_pixels']}",
                fill="black",
            )
    image.save(output / "geometry-comparison.png")


def run(args):
    source = load_source(args.fine, allow_mixed_codes=True)
    owners = face_sources(args.fine, source)
    native = json.loads((args.fine / "native.json").read_text())
    raw = json.loads((args.fine / "mesh/receipt.json").read_text())
    if raw["captures"][0] != raw["captures"][1] or raw["lumped_local_sources"]:
        raise ValueError("matching independent whole/cell Fine streams required")
    xyz = source["positions"].astype(float)
    triangles = xyz[source["indices"]]
    anchor = np.array(native["anchor_ecef"])
    eye = np.array(native["view"]["eye_ecef"]) - anchor
    up = anchor / np.linalg.norm(anchor)
    forward = xyz.mean(0) - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, up)
    right /= np.linalg.norm(right)
    basis = np.stack((right, np.cross(right, forward), forward))
    east = np.cross([0, 0, 1], up)
    east /= np.linalg.norm(east)
    focal = native["view"]["focal_px"]
    views = [
        ("native-eye", (eye, basis, focal, 1280, 720)),
        ("low-eye", low_eye_view(xyz, up, east, np.cross(up, east), focal, 2, 0, 0)),
    ]
    report = {
        "input_root": str(args.fine.resolve()),
        "producer_sha256": raw["binary_sha256"],
        "input_hashes": {
            name: hashlib.sha256((args.fine / name).read_bytes()).hexdigest()
            for name in (
                "input.json",
                "native.json",
                "mesh/receipt.json",
                "subset.index",
                "subset.forms",
            )
        },
        "vertices": len(xyz),
        "triangles": len(triangles),
        "original_sources": source["metadata"]["original_sources"],
        "source_array_bytes": int(
            source["positions"].nbytes + source["indices"].nbytes
        ),
        "radius_m": args.radius_m,
        "width_height": [1280, 720],
        "focal_px": focal,
        "cases": [],
    }
    rows = []
    for name, view in views:
        eye, basis, *_ = view
        blockers, point, region, result = solve(triangles, eye, basis, args.radius_m)
        result["independent_segment_checks"] = verify_segments(
            triangles, blockers, region, eye, args.radius_m
        )
        result["name"] = name
        print(name, json.dumps(result), flush=True)
        masks = [np.zeros(len(triangles), bool), point, region]
        result["views"] = []
        for tag, movement, angle in (
            ("still", 0, 0),
            ("move", args.radius_m, 0),
            ("turn-move", args.radius_m, np.radians(5)),
        ):
            rotated = basis.copy()
            rotated[[0, 2]] = (
                np.array(
                    [[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]]
                )
                @ basis[[0, 2]]
            )
            moved = (eye + movement * basis[0], rotated, *view[2:])
            frames, records = compare(source, masks, moved)
            result["views"].append(
                {
                    "tag": tag,
                    "eye": moved[0].tolist(),
                    "basis": rotated.tolist(),
                    "full_point_region": records,
                }
            )
            if tag == "move":
                rows.append(
                    (f"{name} +{args.radius_m:g}m, cropped diagnostic", frames, records)
                )
        np.savez_compressed(
            args.output / (name + ".npz"),
            blockers=blockers,
            point_omitted=point,
            region_omitted=region,
            face_source=owners,
            eye=eye,
            basis=basis,
            radius_m=args.radius_m,
        )
        report["cases"].append(result)
    report["scope"] = (
        "Unmodified original native Fine triangles treated as opaque; native-eye is original position with cell-facing diagnostic orientation, low-eye is a declared test camera. No actual native CaptureBuildingSurfaces output, Part/Face contract, opacity/PBR or runtime/device admission. Candidate loss retains originals. Single-blocker oracle, not an optimal PVS."
    )
    panel(rows, args.output)
    report["process_peak_rss_kib"] = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fine", type=Path, required=True)
    parser.add_argument("--radius-m", type=float, default=6)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not np.isfinite(args.radius_m) or args.radius_m < 0:
        parser.error("finite nonnegative movement radius required")
    args.output = args.output.resolve()
    if not args.output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private outputs belong in system temp")
    args.output.mkdir(parents=True, exist_ok=False)
    run(args)


if __name__ == "__main__":
    main()
