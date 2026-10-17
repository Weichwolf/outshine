"""Complete positive-vector prototype bank for the upcoming whole-tree diagnostic."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from conifer_window_catalog import family_geometry_for_seed, prototypes, template_image
from foliage_selection import directions
from needle_response_study import source_digest
from normal_lobes import NormalMeasure, quantization_bound
from outshine_ref.vegetation import NeedlePattern


def encode(geometry, bank, index):
    frame = bank["frames"][index]
    measure = NormalMeasure(32, [directions(64)[index]], frame, partitions=(4,))
    projection = tuple(
        bank[key][index] for key in ("frames", "lower", "upper", "offsets")
    )
    image, _ = template_image(
        geometry,
        directions(64)[index],
        32,
        8,
        projection_data=projection,
        normal_measure=measure,
    )
    np.testing.assert_array_equal(image, bank["images"][index])
    field = measure.moments[4][..., 1:] @ frame
    packed = field.astype(np.float32)
    error = quantization_bound(field, packed)
    return (
        packed.reshape(32, 32, 4, 3),
        {
            "visible_samples": measure.sample_count,
            "float32_vector_error_bound_max": float(error.max()),
        },
    )


def build(models, catalog, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    metadata = json.loads((catalog / "catalog.json").read_text())
    source = json.loads((catalog / "source.json").read_text())
    if (
        source["version"] != "finite-window-capacities-1"
        or source["template_subpixel_samples"] != 8
        or len(metadata) != 8
        or sorted(r["needles"] for r in metadata.values())
        != [2, 4, 8, 16, 32, 64, 128, 256]
    ):
        raise ValueError("same bounded eight physical size classes required")
    meshes, paths = prototypes(models, json.loads((models / "shoot.json").read_text()))
    paths += [models / "shoot.json", catalog / "source.json", catalog / "catalog.json"]
    output.mkdir(parents=True, exist_ok=False)
    began = time.perf_counter()
    records = []
    for info in sorted(metadata.values(), key=lambda row: row["family"]):
        path = catalog / f"family-{info['family']}.npz"
        if path.stat().st_size > 16 * 1024**2:
            raise ValueError("bounded bank before decode")
        if info["needles"] * (len(meshes[0][0]["faces"]) + 36) > 250000:
            raise ValueError("source triangle capacity before expansion")
        paths.append(path)
        geometry, _ = family_geometry_for_seed(
            source["canonical_seed"],
            NeedlePattern.from_recipe(info["pattern"]),
            meshes,
            info["needles"],
        )
        with np.load(path, allow_pickle=False) as data:
            bank = {key: data[key] for key in data.files}
        if bank["images"].shape != (64, 32, 32, 9):
            raise ValueError("complete64x32x32bank required")
        vectors, facts = [], []
        for index in range(64):
            v, f = encode(geometry, bank, index)
            vectors.append(v)
            facts.append(f)
        target = output / path.name
        np.savez_compressed(target, vectors=np.array(vectors))
        records.append(
            {
                "family": info["family"],
                "needles": info["needles"],
                "views": 64,
                "file": target.name,
                "sha256": source_digest(target),
                "vector_bytes": sum(v.nbytes for v in vectors),
                "maximum_encoding_bound": max(
                    f["float32_vector_error_bound_max"] for f in facts
                ),
                "visible_samples": sum(f["visible_samples"] for f in facts),
            }
        )
        print(json.dumps(records[-1]), flush=True)
    result = {
        "schema": "diagnostic-positive-vector-bank-1",
        "records": records,
        "source_catalog_sha256": source_digest(catalog / "catalog.json"),
        "input_sha256": {str(p): source_digest(p) for p in paths},
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "wall_s": time.perf_counter() - began,
        "scope": "direct Float32 vectors in each projection frame; no raw-moment codec, shadows, PBR or native asset layout",
    }
    (output / "bank.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--models", type=Path, required=True)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.models, args.catalog, args.output)
