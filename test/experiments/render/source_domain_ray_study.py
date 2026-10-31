"""Finite analytic-domain versus original/candidate triangle coverage diagnostic."""

import argparse
import hashlib
import json
import time
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from prepared_surface_recipes import decode
from source_domain_buffer import restore_source
from source_domain_ray_reference import intersect
from triangle_image_reference import (
    image_rays,
    intersect_source,
    low_eye_view,
    prepare_source,
    visible_pixel_bounds,
)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_recipe(directory, identity):
    index = json.loads((directory / "index.json").read_text())
    found = []
    for cell in index:
        ordinal_path = directory / cell["source_file"]
        if sha(ordinal_path) != cell["source_sha256"]:
            raise ValueError("Bound source membership required")
        ids = np.load(ordinal_path, allow_pickle=False)
        for position in np.flatnonzero(ids == identity):
            path = directory / cell["file"]
            if sha(path) != cell["sha256"]:
                raise ValueError("Bound whole source recipe required")
            surfaces, _ = decode(path.read_bytes())
            if len(ids) != len(surfaces):
                raise ValueError("Complete source/recipe association required")
            found.append(surfaces[int(position)])
    if len(found) != 1 or found[0] is None:
        raise ValueError("Exact actual source recipe required")
    return found[0]


def measure(reference, candidate):
    a, b = np.isfinite(reference), np.isfinite(candidate)
    both = a & b
    error = abs(reference[both] - candidate[both])
    return {
        "reference_hits": int(a.sum()),
        "candidate_hits": int(b.sum()),
        "missed_rays": int((a & ~b).sum()),
        "extra_rays": int((~a & b).sum()),
        "common_rays": int(both.sum()),
        "maximum_common_ray_parameter_error": float(error.max(initial=0)),
        "p95_common_ray_parameter_error": float(np.quantile(error, 0.95))
        if len(error)
        else None,
    }


def run(args):
    if args.output.exists():
        raise ValueError("Fresh output required")
    args.output.mkdir(parents=True)
    files = [
        args.producer / "sample.json",
        args.producer / "sample.npz",
        args.storage / "scalar.npz",
    ]
    files.extend(sorted((args.storage / "recipes").iterdir()))
    input_hashes = {str(path): sha(path) for path in files}
    sample_info = json.loads((args.producer / "sample.json").read_text())
    identity = sample_info["prepared_source_ordinal"]
    with np.load(args.storage / "scalar.npz", allow_pickle=False) as z:
        arrays = {key: z[key] for key in z.files}
    patches = restore_source(
        arrays, identity, source_recipe(args.storage / "recipes", identity)
    )
    with np.load(args.producer / "sample.npz", allow_pickle=False) as z:
        sources = {
            name: {
                "positions": z[name + "_vertices"][:, :3].copy(),
                "indices": z[name + "_faces"].copy(),
            }
            for name in ("original", "candidate")
        }
    prepared = {name: prepare_source(source) for name, source in sources.items()}
    width = height = 96
    panel = Image.new("RGB", (width * 4, (height + 20) * 3), "white")
    painter = ImageDraw.Draw(panel)
    results = []
    for row, yaw in enumerate((0, 45, 120)):
        view = low_eye_view(
            sources["original"]["positions"],
            np.array([0, 1, 0]),
            np.array([1, 0, 0]),
            np.array([0, 0, -1]),
            720,
            3,
            yaw,
            0,
        )
        box = visible_pixel_bounds(sources["original"]["positions"], view)
        origins, directions = image_rays(view, box, width, height, supersample=1)
        references = {
            name: intersect_source(
                source, origins, directions, prepared=prepared[name]
            )[0]
            for name, source in sources.items()
        }
        began = time.process_time()
        plane_depth, _, work = intersect(patches, origins, directions)
        cpu = time.process_time() - began
        against = {
            name: measure(depth, plane_depth) for name, depth in references.items()
        }
        results.append(
            {
                "yaw_degrees": yaw,
                "rays": len(origins),
                "patches": len(patches),
                "domain_work": work,
                "single_domain_cpu_s": cpu,
                "against": against,
            }
        )
        masks = [
            np.isfinite(references["original"]),
            np.isfinite(references["candidate"]),
            np.isfinite(plane_depth),
        ]
        diff = np.zeros((len(plane_depth), 3), np.uint8)
        diff[masks[0] & ~masks[2]] = [255, 0, 0]
        diff[~masks[0] & masks[2]] = [0, 180, 0]
        for col, (title, data) in enumerate(
            zip(
                ("Original", "Candidate", "Domains", "Difference"),
                [
                    *(
                        np.repeat((m * 255).astype(np.uint8)[:, None], 3, axis=1)
                        for m in masks
                    ),
                    diff,
                ],
                strict=True,
            )
        ):
            painter.text(
                (col * width + 2, row * (height + 20) + 3),
                f"{yaw}° {title}",
                fill="black",
            )
            panel.paste(
                Image.fromarray(data.reshape(height, width, 3)),
                (col * width, row * (height + 20) + 20),
            )
    panel.save(args.output / "coverage.png")
    if input_hashes != {str(path): sha(path) for path in files}:
        raise ValueError("Diagnostic inputs changed during the run")
    report = {
        "source_ordinal": identity,
        "sample_sha256": sha(args.producer / "sample.npz"),
        "domain_buffer_sha256": sha(args.storage / "scalar.npz"),
        "input_sha256": input_hashes,
        "views": results,
        "scope": "Finite CPU ray-parameter and coverage diagnostic for one source; planar domains remain unsnapped. No material/normal/PBR, native raster, World, cross-source occlusion, actual GPU time or general error bound. Single domain timing has no same-quality cost comparison and is not a performance claim.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--producer", type=Path, required=True)
    parser.add_argument("--storage", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
