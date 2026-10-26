"""Read-only work/byte audit of repeated position indices; never prune a product."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from process_memory import peak_rss_bytes

FACE_ARRAYS = frozenset(
    (
        "indices",
        "face_normal_words",
        "face_patches",
        "corner_uv",
        "corner_metres",
        "corner_shading_words",
        "corner_colour",
        "face_shading_valid",
        "face_native_stream",
        "corner_swap",
    )
)


def audit(arrays):
    p, ids = arrays["positions"], arrays["indices"]
    if (
        p.dtype != np.float32
        or p.ndim != 2
        or p.shape[1] != 3
        or not 0 < len(p) <= 262144
        or not np.isfinite(p).all()
        or ids.ndim != 2
        or ids.shape[1] != 3
        or ids.dtype.kind not in "iu"
        or not 0 < len(ids) <= 131072
        or ids.min() < 0
        or ids.max() >= len(p)
    ):
        raise ValueError(
            "bounded finite Float32 positions and valid triangle indices required"
        )
    duplicate = (
        (ids[:, 0] == ids[:, 1]) | (ids[:, 0] == ids[:, 2]) | (ids[:, 1] == ids[:, 2])
    )
    xyz = p[ids].astype(float)
    cross = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    zero = (cross == 0).all(1)
    face_bytes = 0
    disagreement = {}
    for key in sorted(FACE_ARRAYS & arrays.keys()):
        value = arrays[key]
        if value.shape[:1] != (len(ids),):
            raise ValueError(
                "declared face arrays must match the reference triangle count"
            )
        face_bytes += value.nbytes
        if key.startswith("corner_") and key != "corner_swap":
            if value.dtype.itemsize != 4 or value.ndim != 3 or value.shape[1] != 3:
                raise ValueError("Float32/UInt32 corner-field words required")
            words = value.view(np.uint32)
            different = np.zeros(len(ids), bool)
            for a, b in ((0, 1), (0, 2), (1, 2)):
                different |= (ids[:, a] == ids[:, b]) & (
                    words[:, a] != words[:, b]
                ).any(1)
            disagreement[key] = int(different.sum())
    count = len(ids)
    live = count - int(duplicate.sum())
    full_bytes = sum(value.nbytes for value in arrays.values())
    fixed_bytes = full_bytes - face_bytes
    selected_face_bytes = face_bytes // count * live
    mapping_bytes = live * (2 if count <= 65536 else 4)
    potential = fixed_bytes + selected_face_bytes + mapping_bytes
    result = {
        "reference_triangles": count,
        "repeated_position_index_triangles": int(duplicate.sum()),
        "zero_cross_double_at_stored_float32_triangles": int(zero.sum()),
        "distinct_index_zero_cross_triangles_retained": int((zero & ~duplicate).sum()),
        "hypothetical_draw_triangles": live,
        "duplicate_positions_with_different_corner_fields": disagreement,
        "array_bytes": {key: value.nbytes for key, value in arrays.items()},
        "reference_array_bytes": full_bytes,
        "reference_face_array_bytes": face_bytes,
        "unchanged_array_bytes": fixed_bytes,
        "hypothetical_selected_face_array_bytes": selected_face_bytes,
        "hypothetical_draw_to_reference_map_bytes": mapping_bytes,
        "hypothetical_live_product_bytes": potential,
        "hypothetical_saved_array_bytes_including_map": full_bytes - potential,
        "both_resident_bytes_sharing_unchanged_arrays": full_bytes
        + selected_face_bytes
        + mapping_bytes,
        "triangle_coordinate_diagnostic_bytes": xyz.nbytes,
    }
    if "face_shading_valid" in arrays:
        valid = arrays["face_shading_valid"]
        if valid.dtype != bool:
            raise ValueError("Boolean original shading qualification required")
        result.update(
            reference_unqualified_shading_triangles=int((~valid).sum()),
            hypothetical_live_unqualified_shading_triangles=int(
                (~valid & ~duplicate).sum()
            ),
        )
    return result


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    reports, elapsed, digests = {}, {}, {}
    for name in ("control", "centre"):
        path = args.product / f"{name}.npz"
        raw = path.read_bytes()
        digests[name] = hashlib.sha256(raw).hexdigest()
        with np.load(path, allow_pickle=False) as archive:
            arrays = {key: archive[key] for key in archive.files}
        started = time.perf_counter()
        reports[name] = audit(arrays)
        elapsed[name] = time.perf_counter() - started
        if hashlib.sha256(path.read_bytes()).hexdigest() != digests[name]:
            raise ValueError("source product changed during the read-only audit")
    result = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "input_file_sha256": digests,
        "variants": reports,
        "audit_seconds_excluding_load": elapsed,
        "process_peak_rss_bytes": peak_rss_bytes(),
        "total_allocation_count": None,
        "total_physical_copied_bytes": None,
        "scope": "Read-only repeated-index audit, no selected product or GPU upload is emitted. Potential draw reduction requires deterministic position-only triangle-FILL raster paths without vertex side effects, conservative rasterization or primitive-ID-dependent consumers. Distinct-index collinear triangles remain. All input XYZ/corner fields/reference triangles remain untouched. Hypothetical bytes include all known face arrays, unchanged arrays and one draw-to-reference map; unknown arrays remain fully charged. Keeping the full reference resident does not save these bytes. No general geometric correspondence bound for a pruned product, no native pixel/depth/C/T/S or time admission.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--product", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
