"""Compare complete cached physical shoots to finite unlit angular view sets."""

import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from foliage_selection import directions
from needle_group_reprojection import compare_images, reproject
from needle_group_source import source_geometry
from needle_group_views import render_view

BASE = Path("build/experiments/vegetation/conifer-crowns")
OUTPUT = Path("build/experiments/vegetation/needle-groups")
HERE = Path(__file__).resolve().parent
IMPLEMENTATION = (
    "needle_group_experiment.py",
    "needle_group_source.py",
    "needle_group_views.py",
    "needle_group_reprojection.py",
    "needle_group_layers.py",
    "needle_group_depth_experiment.py",
    "needle_group_checks.py",
    "plant_depth.py",
    "raster_batch.py",
    "foliage_selection.py",
    "leaf_sheet.py",
    "../../../lib/src/outshine_ref/vegetation/shoot_plan.py",
    "../../../lib/src/outshine_ref/vegetation/vector3.py",
)


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def error_summary(errors):
    return {
        "samples": errors,
        "max_absolute_area_error": max(
            abs(e["coverage_area_relative_error"]) for e in errors
        ),
        "max_coverage_l1_relative": max(e["coverage_l1_relative"] for e in errors),
    }


def screen_sizes(source, cache, sphere, validation):
    rows = []
    for size in (16, 32, 64):
        errors = []
        for direction in validation:
            index = int(np.argmax(sphere @ direction))
            reference = render_view(source, direction, size, samples=8)
            candidate = reproject(cache[index], source, direction, size)
            errors.append(compare_images(reference, candidate))
        rows.append({"viewport_size_pixels": size} | error_summary(errors))
    return rows


def compare(species):
    source = source_geometry(BASE / species)
    measures, sources, timings = [], {}, {}
    views = directions(64)
    for index, direction in enumerate(views):
        began = time.perf_counter()
        sources[index] = render_view(source, direction, 64, samples=4)
        timings[index] = time.perf_counter() - began
    validation = directions(17)
    references = [
        render_view(source, direction, 64, samples=8) for direction in validation
    ]
    for count in (4, 8, 16, 32, 64):
        grid = directions(count)
        # Sphere sets differ with count: bake the actual requested directions.
        cache = (
            [sources[i] for i in range(64)]
            if count == 64
            else [render_view(source, d, 64, samples=4) for d in grid]
        )
        errors = []
        for direction, reference in zip(validation, references, strict=True):
            index = int(np.argmax(grid @ direction))
            rendered = reproject(cache[index], source, direction)
            errors.append(compare_images(reference, rendered))
        measures.append(
            {
                "views": count,
                "triangles_selected": 2,
                "triangles_cache": 2 * count,
            }
            | error_summary(errors)
        )
    cache_path = OUTPUT / f"{species}-views64.npz"
    began = time.perf_counter()
    np.savez_compressed(
        cache_path,
        identity=np.array(source["identity"]),
        axis_path=np.array(source["axis_path"], np.int64),
        organ_ids=np.array(json.dumps(source["ids"])),
        **{
            f"view_{i}_{key}": v
            for i, view in sources.items()
            for key, v in view.items()
            if isinstance(v, np.ndarray)
        },
    )
    return {
        "species": species,
        "plan_identity": source["identity"],
        "axis_path": source["axis_path"],
        "source_triangles": len(source["faces"]),
        "source_organs": len(source["ids"]),
        "organ_ids": source["ids"],
        "input_sha256": source["input_sha256"],
        "view64_build_samples_s": list(timings.values()),
        "cache_write_s": time.perf_counter() - began,
        "cache_path": str(cache_path),
        "cache_sha256": digest(cache_path),
        "cache_compressed_bytes": cache_path.stat().st_size,
        "cache_decoded_array_bytes": sum(
            v.nbytes
            for view in sources.values()
            for v in view.values()
            if isinstance(v, np.ndarray)
        ),
        "comparisons": measures,
        "screen_sizes": screen_sizes(source, sources, views, validation),
        "scope": "same physical shoot, independent view directions; coverage/depth only, actual shaders/shadows and whole-tile cost not approved",
    }


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    records = []
    provenance = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "implementation_sha256": {name: digest(HERE / name) for name in IMPLEMENTATION},
        "source_modified": bool(
            subprocess.check_output(
                [
                    "git",
                    "status",
                    "--porcelain",
                    "--",
                    *[str(HERE / n) for n in IMPLEMENTATION],
                ],
                text=True,
            ).strip()
        ),
    }
    for species in ("picea-abies", "pinus-sylvestris", "pinus-cembra", "larix-decidua"):
        result = compare(species) | provenance
        records.append(result)
        print(
            species,
            [
                (
                    r["views"],
                    r["max_absolute_area_error"],
                    r["max_coverage_l1_relative"],
                )
                for r in result["comparisons"]
            ],
            flush=True,
        )
        (OUTPUT / "angular-report.json").write_text(
            json.dumps(records, indent=2) + "\n"
        )


if __name__ == "__main__":
    main()
