"""Give an existing native box control the already stored lit-study city field."""

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from urban_patches import load


def control_field(product, chunks):
    """Preserve selected cells; unsupported control bounds reject the whole export."""
    chunks = np.asarray(chunks)
    if (
        chunks.ndim != 1
        or chunks.dtype.kind not in "iu"
        or not len(chunks)
        or np.any(chunks < 0)
        or np.any(chunks >= len(product.chunks))
        or len(np.unique(chunks)) != len(chunks)
    ):
        raise ValueError("distinct existing study patches required")
    keys = product.chunks[chunks].astype(np.int64)
    if len(np.unique(keys, axis=0)) != len(keys):
        raise ValueError("selected patch keys must be unique")
    first = keys.min(axis=0) * product.side
    size = (keys.max(axis=0) + 1) * product.side - first
    if np.any(size > 256):
        raise ValueError(
            "native control accepts at most 256 by 256 cells; no truncation"
        )
    if not 0 < product.pitch <= 1024 or not 0 < product.frame[4] <= 1:
        raise ValueError("native control pitch/fill bounds exceeded")
    if (
        not np.isfinite(product.heights).all()
        or np.any(product.heights < 0)
        or np.any(product.heights > 1024)
    ):
        raise ValueError("native control height bounds exceeded")
    heights = product.heights.astype(np.float32)
    if not np.array_equal(heights.astype(np.float64), product.heights):
        raise ValueError(
            "control float heights must preserve the stored values exactly"
        )
    grid = np.zeros((size[1], size[0]), np.float32)
    for chunk, key in zip(chunks, keys, strict=True):
        x, y = key * product.side - first
        grid[y : y + product.side, x : x + product.side] = heights[product.tiers[chunk]]
    grid = grid[::-1]
    header = f"{size[0]} {size[1]} {product.pitch:.17g} {product.frame[4]:.17g}\n"
    text = header + "\n".join(" ".join(f"{h:.9g}" for h in row) for row in grid) + "\n"
    north_west = first + [0, size[1]]
    origin = (north_west * product.frame[3] - product.frame[:2]) * product.frame[2]
    return text, {
        "grid_columns_rows": size.tolist(),
        "grid_float32_bytes": grid.nbytes,
        "selected_patches": chunks.tolist(),
        "selected_cells": int(len(chunks) * product.side**2),
        "occupied_cells": int(np.count_nonzero(grid)),
        "global_first_cell_xy": first.tolist(),
        "native_first_reference_cell_xy": (first + [0, size[1] - 1]).tolist(),
        "native_to_reference": [
            [1, 0, 0, float(origin[0])],
            [0, 0, -1, float(origin[1])],
            [0, 1, 0, 0],
            [0, 0, 0, 1],
        ],
        "coordinates": "Native X=east/columns, Y=up/height, Z=south/rows. Stored north-increasing reference rows are reversed. ENU to EUS is a proper rotation (determinant +1); the origin is the rectangle's north-west corner. Native camera/light must use that same frame; cross-renderer equivalence is not asserted.",
        "scope": "Exact selected occupancy/height data, zero padding between selected patches. Unselected cells are omitted from this diagnostic subset, never evidence of World Empty. Not a native world asset or worldwide/LOD/device admission.",
    }


def run(args):
    output = args.output.resolve()
    if not any(
        output.is_relative_to(root.resolve())
        for root in [Path(tempfile.gettempdir()), Path("/tmp")]
    ):
        raise ValueError("diagnostics require a fresh system-temp output")
    manifest_path, product_path = (
        args.prepared / "manifest.json",
        args.prepared / "product.npz",
    )
    manifest = json.loads(manifest_path.read_text())
    product = load(product_path)
    text, report = control_field(product, manifest["diagnostic"]["chunks"])
    consumer = Path(__file__).parent.parent / "render/urban_fields/NativeUrbanField.h"
    report.update(
        {
            "adapter_producer": subprocess.check_output(
                ["git", "rev-parse", "HEAD"], text=True
            ).strip(),
            "adapter_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            "consumer_header_sha256": hashlib.sha256(consumer.read_bytes()).hexdigest(),
            "prepared_product_sha256": hashlib.sha256(
                product_path.read_bytes()
            ).hexdigest(),
            "prepared_manifest_sha256": hashlib.sha256(
                manifest_path.read_bytes()
            ).hexdigest(),
            "prepared_producer": manifest["producer"],
            "source": manifest["source"],
            "supporting_sources": manifest["supporting_sources"],
            "snapshot_sha256": manifest["snapshot_sha256"],
            "input_field_sha256": hashlib.sha256(text.encode()).hexdigest(),
            "native_execution": "Not run; owned by master/engine using the existing --field control.",
        }
    )
    output.mkdir(parents=True, exist_ok=False)
    (output / "input.field").write_text(text)
    (output / "receipt.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {key: report[key] for key in ["grid_columns_rows", "occupied_cells"]}
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepared", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
