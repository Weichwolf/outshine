"""Pack an exact repeated-index draw subset; canonical source products stay intact."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from duplicate_draw_work import FACE_ARRAYS
from process_memory import peak_rss_bytes

UNCHANGED_ARRAYS = frozenset(
    (
        "positions",
        "input_vertex_map",
        "basis",
        "anchor",
        "source_bounds",
        "accepted_sources",
        "needs_sources",
        "patch_sources",
        "patch_parts",
        "patch_sides",
        "patch_gables",
        "patch_material_codes",
        "uniform_colour",
        "unqualified_shading_sources",
    )
)


def compact(arrays):
    unknown = arrays.keys() - (FACE_ARRAYS | UNCHANGED_ARRAYS)
    if unknown:
        raise ValueError(
            "canonical product has undeclared array roles: "
            + ", ".join(sorted(unknown))
        )
    p, ids = arrays["positions"], arrays["indices"]
    if (
        p.dtype != np.float32
        or p.ndim != 2
        or p.shape[1] != 3
        or len(p) > 262144
        or not np.isfinite(p).all()
        or ids.ndim != 2
        or ids.shape[1] != 3
        or ids.dtype not in (np.dtype("uint16"), np.dtype("uint32"))
        or len(ids) > 131072
        or (ids.size and ids.max() >= len(p))
    ):
        raise ValueError(
            "bounded Float32 positions and UInt16/UInt32 triangles required"
        )
    keep = (
        (ids[:, 0] != ids[:, 1]) & (ids[:, 0] != ids[:, 2]) & (ids[:, 1] != ids[:, 2])
    )
    result = {}
    for key, value in arrays.items():
        if key in FACE_ARRAYS:
            if value.shape[:1] != (len(ids),):
                raise ValueError(
                    "face and corner arrays must retain their labelled triangle axis"
                )
            result[key] = value[keep]
        else:
            result[key] = value
    index_type = np.uint16 if len(ids) <= 65536 else np.uint32
    result["draw_to_reference"] = np.flatnonzero(keep).astype(index_type)
    result["reference_triangle_count"] = np.array(len(ids), np.uint32)
    return result, {
        "reference_triangles": len(ids),
        "draw_triangles": int(keep.sum()),
        "removed_repeated_index_triangles": int((~keep).sum()),
        "index_comparisons": 3 * len(ids),
        "live_face_fields": sorted(FACE_ARRAYS & arrays.keys()),
        "new_face_array_payload_bytes": sum(
            result[k].nbytes for k in FACE_ARRAYS & result.keys()
        ),
        "mapping_bytes": result["draw_to_reference"].nbytes,
        "reference_count_bytes": result["reference_triangle_count"].nbytes,
        "reference_array_bytes": sum(v.nbytes for v in arrays.values()),
        "draw_array_bytes": sum(v.nbytes for v in result.values()),
        "reference_ram_required_to_consume_draw": False,
    }


def verify(reference, draw):
    """Check every old field by original row, including its exact dtype/words."""
    mapping = draw["draw_to_reference"]
    if (
        mapping.ndim != 1
        or mapping.dtype.kind != "u"
        or len(mapping) != len(draw["indices"])
        or int(draw["reference_triangle_count"]) != len(reference["indices"])
        or (
            len(mapping)
            and (
                mapping.max() >= len(reference["indices"])
                or (np.diff(mapping.astype(np.int64)) <= 0).any()
            )
        )
    ):
        raise ValueError("ordered unique draw-to-canonical mapping required")
    for key, original in reference.items():
        expected = original[mapping] if key in FACE_ARRAYS else original
        actual = draw[key]
        if (
            expected.dtype != actual.dtype
            or expected.shape != actual.shape
            or expected.tobytes() != actual.tobytes()
        ):
            raise ValueError("draw changed canonical field words: " + key)
    canonical = reference["indices"]
    removed = np.ones(len(canonical), bool)
    removed[mapping] = False
    duplicate = (
        (canonical[:, 0] == canonical[:, 1])
        | (canonical[:, 0] == canonical[:, 2])
        | (canonical[:, 1] == canonical[:, 2])
    )
    if not np.array_equal(removed, duplicate):
        raise ValueError("draw must remove exactly the repeated-index triangles")


def canonical_faces(draw_faces, mapping, canonical_count, retained_count):
    lookup = np.r_[mapping, canonical_count + np.arange(retained_count)]
    result = np.full(len(draw_faces), -1, np.int64)
    valid = draw_faces >= 0
    result[valid] = lookup[draw_faces[valid]]
    return result


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    variants = {}
    for name in ("control", "centre"):
        source = args.product / f"{name}.npz"
        source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
        with np.load(source, allow_pickle=False) as archive:
            original = {key: archive[key] for key in archive.files}
        start = time.perf_counter()
        draw, report = compact(original)
        packed = time.perf_counter()
        verify(original, draw)
        checked = time.perf_counter()
        target = args.output / f"{name}.npz"
        np.savez_compressed(target, **draw)
        if hashlib.sha256(source.read_bytes()).hexdigest() != source_hash:
            raise ValueError("canonical source product changed during draw packing")
        report.update(
            packing_seconds=packed - start,
            independent_word_check_seconds=checked - packed,
            canonical_file=source.name,
            canonical_file_sha256=source_hash,
            draw_file_sha256=hashlib.sha256(target.read_bytes()).hexdigest(),
            draw_file_bytes=target.stat().st_size,
            array_bytes={key: value.nbytes for key, value in draw.items()},
        )
        variants[name] = report
    result = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "variants": variants,
        "process_peak_rss_bytes": peak_rss_bytes(),
        "total_physical_copied_bytes": None,
        "total_allocation_count": None,
        "scope": "Experimental NPZ draw artifacts only, no World/DB codec or GPU binding change. Canonical full source/reference/function products remain persistent and unchanged; the live draw is self-contained and does not require their permanent RAM residency. Remove only repeated position indices; no vertex merge, normal repair, geometry error bound or distinct-index collinear pruning. Valid only for the agreed position-only triangle raster paths; native fragment/depth/caster and device admission remain separate.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--product", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
