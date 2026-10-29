"""Compare a held coupled field against full native original image queries."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path

import numpy as np
from field_view_geometry import compare_geometry
from field_view_inputs import read_view
from field_view_integrals import summed_field
from field_view_sampling import ImageView, reconstruct


def raw_image(path, channels):
    shape = (720, 1280, channels) if channels != 1 else (720, 1280)
    size = int(np.prod(shape)) * 4
    if not path.exists():
        with gzip.open(path.with_suffix(path.suffix + ".gz"), "rb") as stream:
            raw = stream.read(size + 1)
        if len(raw) != size:
            raise ValueError(f"Unexpected compressed native readback extent: {path}")
        return np.frombuffer(raw, dtype="<f4").reshape(shape)
    if path.stat().st_size != size:
        raise ValueError(f"Unexpected native readback extent: {path}")
    return np.memmap(path, mode="r", dtype="<f4", shape=shape)


def compare(args):
    states = json.loads((args.field / "radiance-result.json").read_text())["cases"]
    held = next(s for s in states if s["query"] == args.held_query)
    atlas = read_view(args.field / "world-view-assets.sqlite", held["field_asset_key"])
    n = atlas["pixels"]
    source_path = args.field / (args.held_query + "-source-field.f32")
    source = np.fromfile(source_path, dtype="<f4").reshape(n, n, 4)
    field = summed_field(source)
    queries = json.loads((args.original / "input.json").read_text())["queries"]
    if args.query_prefix:
        queries = [q for q in queries if q["name"].startswith(args.query_prefix)]
    if not queries:
        raise ValueError("The selected original query set is empty")
    rows, images = [], {}
    for query in queries:
        name = query["name"]
        view = ImageView(
            tuple(query["eye"]), tuple(map(tuple, query["basis"])), query["focal_px"]
        )
        sample = reconstruct(field, atlas["grid"], view, approximate=args.approximate)
        candidate = sample.pop("rgba")
        x, y = sample["origin"]
        height, width = candidate.shape[:2]
        count = query.get("filter_axis_samples", 4) ** 2
        original = raw_image(args.original / f"{name}-source-mesh.ss{count}", 4)
        if not np.isfinite(original).all() or np.any(original < 0):
            raise ValueError("Finite nonnegative original radiance/coverage required")
        truth = np.asarray(original[y : y + height, x : x + width], dtype=float)
        # A crop is an output-storage optimization, never a reduced render or
        # omission of newly exposed pixels outside the card support.
        rgb_mass = np.sum(original[:, :, :3], dtype=float)
        alpha_mass = np.sum(original[:, :, 3], dtype=float)
        if rgb_mass <= 0 or alpha_mass <= 0:
            raise ValueError("A nonempty original colour/coverage opponent is required")
        outside = np.array([rgb_mass, alpha_mass]) - [
            truth[:, :, :3].sum(),
            truth[:, :, 3].sum(),
        ]
        row = {
            "query": name,
            "rgb_relative_l1": float(
                (abs(truth[:, :, :3] - candidate[:, :, :3]).sum() + outside[0])
                / rgb_mass
            ),
            "alpha_relative_l1": float(
                (abs(truth[:, :, 3] - candidate[:, :, 3]).sum() + outside[1])
                / alpha_mass
            ),
            "original_mass_outside_support": outside.tolist(),
            "sampling": sample,
        }
        normal = args.original / (name + "-source-mesh.normal")
        if normal.exists() or normal.with_suffix(normal.suffix + ".gz").exists():
            row["geometry"] = compare_geometry(
                atlas,
                view,
                raw_image(args.original / (name + "-source-mesh.depth"), 1),
                raw_image(normal, 4),
            )
        rows.append(row)
        images[name + "_original"], images[name + "_field"] = truth, candidate
        images[name + "_origin"] = np.array([x, y])
    args.output.mkdir(parents=True, exist_ok=False)
    np.savez_compressed(args.output / "images.npz", **images)
    report = {
        "field_key": atlas["key"],
        "raw_view_sha256": atlas["sha256"],
        "source_radiance_sha256": hashlib.sha256(source_path.read_bytes()).hexdigest(),
        "raw_view_bytes": atlas["raw_bytes"],
        "prefix_bytes": field.table.nbytes,
        "held_query": args.held_query,
        "profile": [1280, 720],
        "rows": rows,
        "scope": "Finite original subpixel averages, not a continuous visibility/radiance certificate. One held linear premultiplied first-layer field, no dropped source or main-depth claim.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "queries": len(rows),
                "maximum_rgb_relative_l1": max(r["rgb_relative_l1"] for r in rows),
                "maximum_alpha_relative_l1": max(r["alpha_relative_l1"] for r in rows),
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("field", "original", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--held-query", required=True)
    parser.add_argument("--query-prefix", default="")
    parser.add_argument("--approximate", action="store_true")
    compare(parser.parse_args())


if __name__ == "__main__":
    main()
