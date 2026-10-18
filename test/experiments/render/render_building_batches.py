"""Whole-cohort software depth before/after packing; no native lit-world claim."""

import argparse
import hashlib
import json
import tempfile
from pathlib import Path

import numpy as np
from cached_building_streams import STREAM_BYTES, basis_sources, product
from compare_building_batches import checked, peak_memory
from current_visibility import camera, project, raster
from PIL import Image


def mesh(streams):
    vertices, triangles, offset = [], [], 0
    for positions, clusters, index, fallback in ((0, 4, 7, 2), (1, 8, 9, 3)):
        points = np.frombuffer(streams[positions], dtype="<f4").reshape(-1, 6)[:, :3]
        records = np.frombuffer(streams[clusters], dtype="<u4").reshape(-1, 13)
        indices = np.frombuffer(streams[index] or streams[fallback], dtype="<u4")
        if len(records):
            if (records[:, 12] != 0).any():
                raise ValueError(
                    "this diagnostic requires already selected level-0 clusters"
                )
            runs = [
                indices[first : first + count] for first, count in records[:, 10:12]
            ]
            indices = np.concatenate(runs)
        if not np.isfinite(points).all():
            raise ValueError("finite native positions required")
        vertices.append(points)
        triangles.append(indices.astype(np.int64).reshape(-1, 3) + offset)
        offset += len(points)
    return np.concatenate(vertices), np.concatenate(triangles)


def run(inputs, packed, parent, output):
    manifest = json.loads((inputs / "manifest.json").read_text())
    spec = next(s for s in manifest["cohorts"] if s["parent"] == parent)
    basis = next(b for b in manifest["basis"] if b["key"] == spec["basis"])
    anchor, _ = basis_sources(checked(inputs / basis["file"], basis["sha256"]))
    report = json.loads((packed / parent / "report.json").read_text())
    originals = [
        product(checked(inputs / f"{r['key']}.structure", r["sha256"]), r["key"])
        for r in spec["products"]
    ]
    source = [mesh(p["streams"]) for p in originals]
    pages = [
        mesh(
            [
                (packed / parent / f"page-{i}-stream-{j}.bin").read_bytes()
                for j in range(len(STREAM_BYTES))
            ]
        )
        for i in range(len(report["pages"]))
    ]
    reference_vertices = np.concatenate([v for v, _ in source])
    scene = {
        "anchor_ecef": anchor,
        "eye_ecef": report["view"]["eye_ecef"],
        "focal_px": report["view"]["focal_px"],
    }
    cases = []
    for yaw in (0.0, np.pi / 2, np.pi, 3 * np.pi / 2):
        view = camera(scene, reference_vertices, 1280, 720, yaw)
        images = []
        for name, models in (("original", source), ("packed", pages)):
            depth = np.zeros((720, 1280))
            for vertices, triangles in models:
                projected, z = project(vertices, view)
                depth = np.maximum(depth, raster(projected, z, triangles, 1280, 720))
            images.append(depth)
            shade = np.where(
                depth > 0, 255 * np.sqrt(np.clip(depth * 100, 0, 1)), 0
            ).astype(np.uint8)
            Image.fromarray(shade).save(
                output / f"yaw-{round(np.degrees(yaw))}-{name}.png"
            )
        if not np.array_equal(images[0], images[1]):
            raise ValueError("complete cohort depth differs after packing")
        if yaw == 0 and not (images[0] > 0).any():
            raise ValueError(
                "empty forward image cannot establish visible geometry preservation"
            )
        cases.append(
            {
                "yaw_deg": float(np.degrees(yaw)),
                "covered_pixels": int((images[0] > 0).sum()),
                "changed_depth_pixels": 0,
                "depth_sha256": hashlib.sha256(images[0].tobytes()).hexdigest(),
            }
        )
    (output / "report.json").write_text(
        json.dumps(
            {
                "parent": parent,
                "profile": [1280, 720],
                "cases": cases,
                "memory": peak_memory(),
                "scope": "Same explicit CPU geometry camera, two whole-cohort depth images. Pixel-centre opaque level-0 triangles, near-crossing omitted. No MSAA, alpha, material/light/shadow, full-world, runtime LOD/visibility or native/device proof.",
            },
            indent=2,
        )
        + "\n"
    )
    print(json.dumps(cases), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--packed", type=Path, required=True)
    parser.add_argument("--parent", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    target, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    checkout = Path(__file__).resolve().parents[3]
    if (
        target == temporary
        or not target.is_relative_to(temporary)
        or target.is_relative_to(checkout)
        or target.exists()
    ):
        parser.error("fresh explicit system-temp output outside the checkout required")
    target.mkdir(parents=True)
    run(args.inputs.resolve(), args.packed.resolve(), args.parent, target)
