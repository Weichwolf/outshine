"""A standalone finished rendering prototype, separate from catalogue definitions."""

import argparse
import hashlib
import json
import shutil
import time
from pathlib import Path

import numpy as np
from stand_leaf_pack import pack_leaves
from tree_entry import load


def source_files(inputs, metadata):
    names = [
        "entry.json",
        metadata["tables"],
        *metadata["products"],
        "fern-material.json",
        "fern-colour.png",
        "fern-height.png",
    ]
    return {
        name: hashlib.sha256((inputs / name).read_bytes()).hexdigest()
        for name in sorted(set(names))
    }


def build(inputs, output):
    start = time.perf_counter()
    metadata, source = load(inputs)
    loaded = time.perf_counter()
    rows = source[metadata["tables"]]
    meshes = [source[f"blade-{i}.npz"] for i in range(8)]
    mesh, runs, report = pack_leaves(rows, meshes)
    packed = time.perf_counter()
    fingerprint = hashlib.sha256(
        json.dumps(source_files(inputs, metadata), sort_keys=True).encode()
    ).hexdigest()
    output.mkdir(parents=True, exist_ok=False)
    np.savez_compressed(output / "leaf-mesh.npz", **mesh)
    np.savez_compressed(output / "leaf-runs.npz", **runs)
    for name in (
        "wood.npz",
        "fern-material.json",
        "fern-colour.png",
        "fern-height.png",
    ):
        shutil.copyfile(inputs / name, output / name)
    finished = {
        **metadata,
        "format": "packed-crown-rendering-1",
        "source_product_sha256": fingerprint,
        "tables": "leaf-runs.npz",
        "products": ["wood.npz", "leaf-mesh.npz"],
        "packing": report,
    }
    finished["product_sha256"] = {
        f.name: hashlib.sha256(f.read_bytes()).hexdigest()
        for f in sorted(output.iterdir())
    }
    finished["identity"] = hashlib.sha256(
        json.dumps(finished, sort_keys=True).encode()
    ).hexdigest()
    (output / "entry.json").write_text(json.dumps(finished, indent=2) + "\n")
    stages = {
        "source_decode_s": loaded - start,
        "packing_s": packed - loaded,
        "fingerprint_export_s": time.perf_counter() - packed,
    }
    (output / "build-report.json").write_text(
        json.dumps(stages | report, indent=2) + "\n"
    )
    print(json.dumps(stages | report), flush=True)


def ready(inputs, expected_source=None):
    metadata = json.loads((inputs / "entry.json").read_text())
    if metadata["format"] != "packed-crown-rendering-1":
        raise ValueError("known finished rendering product required")
    identity = {k: v for k, v in metadata.items() if k != "identity"}
    if (
        hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
        != metadata["identity"]
    ):
        raise ValueError("finished rendering metadata changed")
    if (
        expected_source is not None
        and metadata["source_product_sha256"] != expected_source
    ):
        raise ValueError(
            "finished rendering product belongs to different source inputs"
        )
    for name, digest in metadata["product_sha256"].items():
        if hashlib.sha256((inputs / name).read_bytes()).hexdigest() != digest:
            raise ValueError("finished rendering product changed: " + name)
    return load(inputs)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.inputs, args.output)
