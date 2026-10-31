"""Bound existing native views against original/candidate source groups.

Counts experimental indexed group reads. This is view-only, not shadow,
simulation, native raster, World residency, physical IO or device admission.
"""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from native_source_clip_controls import read as read_controls
from native_source_triangle_materials import DATA, bound_selection
from source_clip_demand import outside_xy
from source_cluster_bounds import TINY32, frame
from source_domain_chunk_study import decode_raw
from source_patch_inputs import read_bound_sources
from source_patch_product import read_product

ROOT = Path(__file__).resolve().parents[3]


def stable_transform(points, matrix):
    result = np.zeros((len(points), 4), np.float32)
    for i in range(4):
        result = np.add(
            result,
            np.multiply(points[:, i, None], matrix[:, i], dtype=np.float32),
            dtype=np.float32,
        )
    return result


def check_excluded(points, operands, plane):
    placement, shift, matrix = (x.astype(np.float32) for x in operands)
    homogeneous = np.c_[points, np.ones(len(points), np.float32)]
    world = np.add(stable_transform(homogeneous, placement), shift, dtype=np.float32)
    clip = stable_transform(world, matrix)
    x, y, w = clip[:, 0], clip[:, 1], clip[:, 3]
    tests = (x + w, w - x, y + w, w - y)
    if not np.isfinite(clip).all() or not np.all(tests[plane] < 0):
        raise ValueError("Independent FP32 original/candidate exclusion counterexample")


def main(args):
    source, context, provenance = read_bound_sources(
        ROOT / "test/outshine/data/render/building-depth-samples"
    )
    bound_selection(DATA, source)
    producer = json.loads((args.producer / "report.json").read_text())
    if any(
        producer["input_provenance"][k] != provenance[k]
        for k in ("source_binding_sha256", "source_archive_sha256")
    ):
        raise ValueError("Identical actual source cohort required")
    parts = read_product(
        args.producer / "candidate",
        producer["diagnostic_native_candidate"],
        context["source_ordinals"],
    )
    old_ids = context["triangle_binding"][:, 1]
    new_ids = np.concatenate([ids for _, _, ids in parts])
    new_faces = np.concatenate(
        [values["positions"][values["indices"]] for _, values, _ in parts]
    )
    old_faces = source["positions"][source["indices"]]
    index = np.load(args.chunk / "index.npy", allow_pickle=False)
    records = {
        int(row["source"]): (int(row["offset"]), int(row["bytes"])) for row in index
    }
    with np.load(DATA / "native-visible-triangles.npz", allow_pickle=False) as archive:
        controls, _ = read_controls(archive["near-direction/view_words"])
        placements = archive["placements/words"].copy()
    groups = {}
    for identity in map(int, context["source_ordinals"]):
        points = np.concatenate(
            (
                old_faces[old_ids == identity].reshape(-1, 3),
                new_faces[new_ids == identity].reshape(-1, 3),
            )
        )
        if not len(points) or not np.isfinite(points).all():
            raise ValueError("Complete finite original and candidate required")
        bounds = np.stack((points.min(axis=0), points.max(axis=0))).astype("<f4")
        normal = not np.any((np.abs(points) > 0) & (np.abs(points) < TINY32))
        groups[identity] = bounds, points, normal
    if not set(records).issubset(groups):
        raise ValueError("Only bound candidate source identities may be indexed")
    if args.bounds_output:
        chunks = sorted(set(records.values()))
        digests = []
        with (args.chunk / "groups.bin").open("rb") as stream:
            for offset, length in chunks:
                stream.seek(offset)
                blob = stream.read(length)
                if len(blob) != length:
                    raise ValueError("Complete original group data required")
                digests.append(np.frombuffer(hashlib.sha256(blob).digest(), dtype="u1"))
        np.savez(
            args.bounds_output,
            source_ids=np.array(list(groups), dtype="<i4"),
            bounds=np.array([v[0] for v in groups.values()], dtype="<f4"),
            normal=np.array([v[2] for v in groups.values()], dtype="u1"),
            source_binding=np.frombuffer(
                bytes.fromhex(provenance["source_binding_sha256"]), dtype="u1"
            ),
            index_digest=np.frombuffer(
                hashlib.sha256((args.chunk / "index.npy").read_bytes()).digest(),
                dtype="u1",
            ),
            group_offsets=np.array([row[0] for row in chunks], dtype="<u8"),
            group_lengths=np.array([row[1] for row in chunks], dtype="<u4"),
            group_digests=np.array(digests, dtype="u1"),
        )
    cases = []
    for angle, view in sorted(controls.items()):
        operands = frame(view, placements)
        selected, excluded = [], 0
        if operands is None:
            raise ValueError("Recorded finite view required")
        for identity, (bounds, points, normal) in groups.items():
            upper = outside_xy(bounds, operands) if normal else None
            planes = np.flatnonzero(upper < 0) if upper is not None else []
            if len(planes):
                check_excluded(points, operands, int(planes[0]))
                excluded += 1
            else:
                selected.append(identity)
        chunks = {records[i] for i in selected if i in records}
        read_bytes, decoded_ids = 0, set()
        with (args.chunk / "groups.bin").open("rb") as stream:
            for offset, length in sorted(chunks):
                stream.seek(offset)
                blob = stream.read(length)
                if len(blob) != length:
                    raise ValueError("Incomplete group must retain original")
                decoded_ids.update(map(int, decode_raw(blob)["source_ids"]))
                read_bytes += len(blob)
        if not {i for i in selected if i in records}.issubset(decoded_ids):
            raise ValueError("Every demanded candidate source must be present")
        cases.append(
            {
                "angle_degrees": angle,
                "selected_sources": len(selected),
                "excluded_sources": excluded,
                "selected_original_fallback_sources": sum(
                    i not in records for i in selected
                ),
                "unique_groups": len(chunks),
                "group_bytes": read_bytes,
                "index_plus_group_bytes": (args.chunk / "index.npy").stat().st_size
                + read_bytes,
            }
        )
    report = {
        "source_binding_sha256": provenance["source_binding_sha256"],
        "source_count": len(groups),
        "original_triangles": len(old_faces),
        "candidate_triangles": len(new_faces),
        "index_bytes": (args.chunk / "index.npy").stat().st_size,
        "cases": cases,
        "scope": "Recorded finite native view words, conservative union of diagnostic original/candidate bounds, scalar-lane FP32 corner controls and actual selected group reads. Bounds are constructed offline from all original/candidate triangles, so these read counters exclude the full preparation. View-only; unsupported original fallbacks are counted but their bytes, recipes, native raster, shadows, simulation, live World, device timing and physical IO are not covered.",
    }
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--producer", type=Path, required=True)
    parser.add_argument("--chunk", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--bounds-output", type=Path)
    main(parser.parse_args())
