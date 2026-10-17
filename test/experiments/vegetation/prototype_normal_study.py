"""Capture positive moments while rastering the same actual canonical prototypes."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from conifer_window_catalog import family_geometry_for_seed, prototypes, template_image
from foliage_selection import directions
from needle_response_study import source_digest
from normal_lobe_plot import plot
from normal_lobe_study import analyse, angular_queries, coarsen
from normal_lobes import NormalMeasure
from outshine_ref.vegetation import NeedlePattern


def capture(geometry, bank, index, output):
    frame = bank["frames"][index]
    view = directions(64)[index]
    queries = angular_queries(view)
    measure = NormalMeasure(32, queries, frame)
    projection = tuple(
        bank[key][index] for key in ("frames", "lower", "upper", "offsets")
    )
    began = time.perf_counter()
    image, _ = template_image(
        geometry, view, 32, 8, projection_data=projection, normal_measure=measure
    )
    seconds = time.perf_counter() - began
    np.testing.assert_array_equal(image, bank["images"][index])
    np.testing.assert_allclose(
        measure.moments[4][..., 0].sum(axis=1),
        image[..., 3].ravel(),
        atol=1e-14,
        rtol=0,
    )
    np.testing.assert_allclose(
        measure.moments[4][..., 1:].sum(axis=1),
        image[..., 4:7].reshape(-1, 3),
        atol=1e-7,
        rtol=0,
    )
    arrays = {"directions": queries, "unchanged_image": image}
    levels = []
    size = 32
    moments, exact = measure.moments, measure.exact
    while True:
        levels.append(analyse(moments, exact, queries, size, arrays))
        if size == 1:
            break
        moments = {n: coarsen(field, size) for n, field in moments.items()}
        exact = coarsen(exact.T, size).T
        size //= 2
    output.mkdir()
    np.savez_compressed(output / "fields.npz", **arrays)
    plot(arrays, output / "comparison.png", 32)
    report = {
        "view_index": index,
        "view": view.tolist(),
        "partition_frame": frame.tolist(),
        "normal_samples": measure.sample_count,
        "capture_wall_s": seconds,
        "levels": levels,
        "source_geometry_bytes": sum(a.nbytes for a in geometry.values()),
        "source_triangles": len(geometry["faces"]),
        "same_cached_image": True,
        "source_measure": "existing 256x256 front raster, not continuous area",
        "moment_payload_bytes": sum(a.nbytes for a in measure.moments.values()),
        "exact_reference_bytes": measure.exact.nbytes,
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def run(models, catalog, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    metadata = json.loads((catalog / "catalog.json").read_text())
    source = json.loads((catalog / "source.json").read_text())
    if (
        source["version"] != "finite-window-capacities-1"
        or source["template_subpixel_samples"] != 8
    ):
        raise ValueError(
            "same explicit physical32x32/8sample canonical catalogue required"
        )
    selected = {
        row["needles"]: row
        for row in metadata.values()
        if row["needles"] in (256, 32, 2)
    }
    if set(selected) != {256, 32, 2}:
        raise ValueError("existing256/32/2-capacity products required")
    meshes, paths = prototypes(models, json.loads((models / "shoot.json").read_text()))
    paths += [models / "shoot.json", catalog / "source.json", catalog / "catalog.json"]
    output.mkdir(parents=True, exist_ok=False)
    records = []
    for capacity, info in selected.items():
        path = catalog / f"family-{info['family']}.npz"
        if path.stat().st_size > 16 * 1024**2:
            raise ValueError("bounded selected bank before decode")
        paths.append(path)
        pattern = NeedlePattern.from_recipe(info["pattern"])
        if capacity * (len(meshes[0][0]["faces"]) + 36) > 250000:
            raise ValueError("source triangle budget before prototype expansion")
        geometry, facts = family_geometry_for_seed(
            source["canonical_seed"], pattern, meshes, capacity
        )
        if facts["phase"] != info["phase"]:
            raise ValueError("same source canonical phase required")
        with np.load(path, allow_pickle=False) as data:
            bank = {key: data[key] for key in data.files}
        if bank["images"].shape != (64, 32, 32, 9):
            raise ValueError("complete existing64-view bank required")
        for index in (0, 19, 47):
            record = capture(geometry, bank, index, output / f"{capacity}-{index}")
            records.append(
                {
                    "capacity": capacity,
                    "family": info["family"],
                    "decoded_bank_bytes": sum(a.nbytes for a in bank.values()),
                    **record,
                }
            )
            print(
                json.dumps(
                    {
                        "capacity": capacity,
                        "view": index,
                        "samples": record["normal_samples"],
                    }
                ),
                flush=True,
            )
    result = {
        "schema": "outshine-prototype-normal-study-1",
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "input_sha256": {str(p): source_digest(p) for p in paths},
        "records": records,
        "scope": "same finite raster support, unshadowed diffuse only; no full bank or whole tree admission",
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--models", type=Path, required=True)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.models, args.catalog, args.output)
