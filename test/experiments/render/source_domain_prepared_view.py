"""Select experimental groups from prepared whole-source bounds, without fine geometry."""

import argparse
import hashlib
import io
import json
from pathlib import Path

import numpy as np
from native_source_clip_controls import read as read_controls
from native_source_triangle_materials import DATA
from source_clip_demand import outside_xy_batched
from source_cluster_bounds import frame
from source_domain_chunk_study import decode_raw


def sha(data):
    return hashlib.sha256(data).digest()


def select_sources(prepared, view, placements):
    """Keep complete sources unless the bound proves they are outside."""
    if len(np.unique(prepared["source_ids"])) != len(prepared["source_ids"]):
        raise ValueError("Unique prepared source identities required")
    operands = frame(view, placements)
    if len(prepared["source_ids"]) != len(prepared["bounds"]):
        raise ValueError("One bound per prepared source required")
    excluded = outside_xy_batched(prepared["bounds"], prepared["normal"], operands)
    return list(map(int, prepared["source_ids"][~excluded]))


def run(args, *, retain_angle=None):
    raw = args.bounds.read_bytes()
    if sha(raw).hex() != args.bounds_sha256:
        raise ValueError("Prepared bounds identity unknown; retain originals")
    with np.load(io.BytesIO(raw), allow_pickle=False) as z:
        prepared = {key: z[key] for key in z.files}
    index_raw = (args.chunk / "index.npy").read_bytes()
    if sha(index_raw) != prepared["index_digest"].tobytes():
        raise ValueError("Index identity unknown; retain originals")
    index = np.load(io.BytesIO(index_raw), allow_pickle=False)
    records = {int(r["source"]): (int(r["offset"]), int(r["bytes"])) for r in index}
    hashes = {
        (int(o), int(n)): h.tobytes()
        for o, n, h in zip(
            prepared["group_offsets"],
            prepared["group_lengths"],
            prepared["group_digests"],
            strict=True,
        )
    }
    if len(records) != len(index) or set(records.values()) != set(hashes):
        raise ValueError("Complete unique prepared group index required")
    with np.load(DATA / "native-visible-triangles.npz", allow_pickle=False) as z:
        views, _ = read_controls(z["near-direction/view_words"])
        placements = z["placements/words"].copy()
    rows = []
    retained = {}
    retained_sources = None
    for angle, view in sorted(views.items()):
        if getattr(args, "angle", None) is not None and args.angle != angle:
            continue
        selected = select_sources(prepared, view, placements)
        if angle == retain_angle:
            retained_sources = tuple(selected)
        chunks = {records[i] for i in selected if i in records}
        received, found = 0, set()
        with (args.chunk / "groups.bin").open("rb") as stream:
            for offset, length in sorted(chunks):
                stream.seek(offset)
                blob = stream.read(length)
                if len(blob) != length or sha(blob) != hashes[offset, length]:
                    raise ValueError("Group incomplete or changed; retain originals")
                if angle == retain_angle:
                    retained[offset, length] = blob
                received += len(blob)
                found.update(map(int, decode_raw(blob)["source_ids"]))
        if not {i for i in selected if i in records}.issubset(found):
            raise ValueError("Incomplete selected group; retain originals")
        fallback_ids = {i for i in selected if i not in records}
        fallback_bytes = 0
        if fallback_ids:
            fallback_raw = args.fallback.read_bytes()
            if sha(fallback_raw) != prepared["fallback_digest"].tobytes():
                raise ValueError("Original fallback identity unknown; retain originals")
            with np.load(io.BytesIO(fallback_raw), allow_pickle=False) as z:
                available = set(map(int, z["actual_triangle_binding"][:, 1]))
                if not fallback_ids.issubset(available):
                    raise ValueError("Whole original fallback unavailable")
                for key in ("corners", "material", "actual_triangle_binding"):
                    if not len(z[key]):
                        raise ValueError("Whole original fallback data required")
            fallback_bytes = len(fallback_raw)
        rows.append(
            {
                "angle_degrees": angle,
                "selected_sources": len(selected),
                "selected_source_ids": selected,
                "selected_original_fallback_sources": sum(
                    i not in records for i in selected
                ),
                "unique_groups": len(chunks),
                "group_bytes": received,
                "fallback_bytes": fallback_bytes,
                "metadata_index_plus_group_bytes": len(raw)
                + len(index_raw)
                + received
                + fallback_bytes,
            }
        )
    result = {
        "bounds_sha256": sha(raw).hex(),
        "bounds_file_bytes": len(raw),
        "index_file_bytes": len(index_raw),
        "cases": rows,
        "scope": "Fresh Python process reads prepared metadata, index, demanded groups and the complete known original fallback if selected. Bounds, fallback identity and per-group hashes were verified offline from complete source/candidate arrays. View-only; live World/revisions, recipe bytes, shadows, simulation, native raster, physical IO and device time are not covered. Integrity failure raises for whole-original fallback; caller integration is not implemented.",
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))
    return retained, retained_sources


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bounds", type=Path, required=True)
    parser.add_argument("--bounds-sha256", required=True)
    parser.add_argument("--chunk", type=Path, required=True)
    parser.add_argument("--fallback", type=Path, required=True)
    parser.add_argument("--angle", type=int)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
