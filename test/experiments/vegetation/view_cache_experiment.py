"""Fresh-process complete view-cache load and CPU selection through one camera turn."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from cloud_product import view_indices, view_snapshot
from PIL import Image


def load_product(directory):
    began = time.perf_counter()
    manifest = json.loads((directory / "cloud-product.json").read_text())
    recipe = json.loads((directory / manifest["recipe"]).read_text())
    identity = hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
    assert (
        manifest["version"] == "spherical-group-views-1"
        and identity == manifest["plan_identity"]
    )
    with np.load(directory / manifest["cache_mesh"], allow_pickle=False) as stored:
        mesh = {key: stored[key] for key in stored.files}
    for value in mesh.values():
        value.setflags(write=False)
        assert np.all(np.isfinite(value))
    records = json.loads((directory / "cloud-groups.json").read_text())
    assert len(records) == 630 and all(
        record["charts"] == manifest["complete_views_per_group"] for record in records
    )
    ids = np.concatenate(
        [
            np.arange(record["first_organ"], record["first_organ"] + record["organs"])
            for record in records
        ]
    )
    np.testing.assert_array_equal(ids, np.arange(37800))
    images = []
    for filename in manifest["product"]["textures"].values():
        with Image.open(directory / filename) as image:
            images.append(np.asarray(image.convert("RGBA")))
    return mesh, records, images, (time.perf_counter() - began) * 1000


def loop_indices(mesh, records, camera):
    selected = []
    for record in records:
        first, count = record["first_chart"], record["charts"]
        direction = np.asarray(camera) - record["anchor_origin_m"]
        normals = mesh["normals"].reshape(-1, 6, 3)[first : first + count, 0]
        selected.append(first + int(np.argmax(normals @ direction)))
    return np.asarray(selected)


def measure(mesh, records, frames):
    reference, indices = view_snapshot(mesh, records, (20, -40, 15))
    reverse, reversed_indices = view_snapshot(
        mesh, list(reversed(records)), (20, -40, 15)
    )
    np.testing.assert_array_equal(indices, reversed_indices[::-1])
    np.testing.assert_array_equal(
        reference["vertices"].reshape(-1, 4, 3),
        reverse["vertices"].reshape(-1, 4, 3)[::-1],
    )
    shadow, shadow_indices = view_snapshot(mesh, records, view_direction=(0, 0, 1))
    assert len(shadow["faces"]) == len(reference["faces"])
    assert np.any(shadow_indices != indices)
    times, loop_times, vector_times, selections = [], [], [], []
    for angle in np.arange(frames) * 2 * np.pi / frames:
        camera = (
            20 * np.cos(angle) + 40 * np.sin(angle),
            20 * np.sin(angle) - 40 * np.cos(angle),
            15,
        )
        began = time.perf_counter()
        selected, chosen = view_snapshot(mesh, records, camera)
        times.append((time.perf_counter() - began) * 1000)
        assert len(selected["faces"]) == 1260 and len(np.unique(chosen)) == len(records)
        assert len(selected["vertices"]) == 2520 and np.all(
            np.isfinite(selected["vertices"])
        )
        began = time.perf_counter()
        reference_indices = loop_indices(mesh, records, camera)
        loop_times.append((time.perf_counter() - began) * 1000)
        began = time.perf_counter()
        vector_indices = view_indices(mesh, records, camera)
        vector_times.append((time.perf_counter() - began) * 1000)
        np.testing.assert_array_equal(reference_indices, vector_indices)
        np.testing.assert_array_equal(chosen, reference_indices)
        selections.append(chosen.tolist())
    return {
        "frames": frames,
        "p50_ms": float(np.percentile(times, 50)),
        "p95_ms": float(np.percentile(times, 95)),
        "p99_ms": float(np.percentile(times, 99)),
        "samples_ms": times,
        "loop_index_samples_ms": loop_times,
        "batch_index_samples_ms": vector_times,
        "batch_matches_loop_indices": True,
        "ordered_and_reversed_requests_agree": True,
        "light_direction_has_independent_selection": True,
        "selected_charts": selections,
        "draw_leaf_triangles": len(reference["faces"]),
        "draw_mesh_array_bytes": sum(value.nbytes for value in reference.values()),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--directory", type=Path, default=Path("build/experiments/vegetation/selection")
    )
    parser.add_argument("--frames", type=int, default=30)
    args = parser.parse_args()
    if not 1 <= args.frames <= 120:
        parser.error("one to 120 camera samples required")
    mesh, records, images, load_ms = load_product(args.directory)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--", str(Path(__file__).parent)],
                text=True,
            ).strip()
        ),
        "cached_groups": len(records),
        "cached_charts": len(mesh["vertices"]) // 4,
        "cached_mesh_array_bytes": sum(value.nbytes for value in mesh.values()),
        "decoded_atlas_bytes": sum(value.nbytes for value in images),
        "load_ms": load_ms,
        "selection": measure(mesh, records, args.frames),
        "process_peak_rss_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "scope": "fresh Linux process, complete CPU cache decode and exactly 30 camera selections when frames=30; no generation, native frame or GPU measurement",
    }
    (args.directory / "view-cache.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {key: value for key, value in report.items() if key != "selection"}
            | {
                "selection_ms": {
                    key: report["selection"][key]
                    for key in ("p50_ms", "p95_ms", "p99_ms")
                }
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
