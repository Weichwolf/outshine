"""Replay only draw index width on a complete exported scene, without rebuilding it."""

import argparse
import hashlib
import json
import time
from pathlib import Path

import numpy as np
from evidence import revision
from podium_probe import write
from terraced_city_parts import fingerprint
from tile_terraced import indices


def replay(geometry, output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the index replay before evidence")
    record = json.loads(geometry.read_text())
    source = Path(record["mesh"])
    if hashlib.sha256(source.read_bytes()).hexdigest() != record["mesh_sha256"]:
        raise ValueError("Original whole scene no longer matches its saved mesh")
    with np.load(source) as stored:
        old = dict(stored)
    start = time.perf_counter()
    faces = indices(old)
    elapsed = time.perf_counter() - start
    if not np.array_equal(old["faces"], faces):
        raise ValueError("Narrowed draw indices changed actual geometry")
    actual = old | {"faces": faces}
    for key, value in old.items():
        if not np.array_equal(value, actual[key]):
            raise ValueError("Index replay changed " + key)
    metadata = {
        k: v
        for k, v in record.items()
        if k not in proof
        and k not in ("mesh", "mesh_sha256", "triangles", "assembly_seconds")
    }
    metadata["index_replay"] = {
        "original_geometry": str(geometry.resolve()),
        "original_geometry_sha256": hashlib.sha256(geometry.read_bytes()).hexdigest(),
        "original_commit": record["commit"],
        "original_mesh_array_sha256": record["mesh_array_sha256"],
        "original_assembly_seconds": record["assembly_seconds"],
        "old_index_dtype": old["faces"].dtype.str,
        "new_index_dtype": faces.dtype.str,
        "old_index_bytes": old["faces"].nbytes,
        "new_index_bytes": faces.nbytes,
        "conversion_seconds": elapsed,
        "all_actual_values_exact": True,
        "scope": "Index-width-only replay of the complete earlier scene; no new modelling/physical construction or changed camera/material/float fields.",
    }
    metadata["mesh_array_sha256"] = fingerprint(actual)
    write(output, record["city"], actual, metadata, proof)
    print(json.dumps(metadata["index_replay"]), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--geometry", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve() == Path(
        "/tmp"
    ) or not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("--output requires an explicit subdirectory of /tmp")
    args.output.mkdir(parents=True, exist_ok=True)
    replay(args.geometry, args.output)


if __name__ == "__main__":
    main()
