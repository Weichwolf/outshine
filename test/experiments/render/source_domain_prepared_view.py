"""Select experimental groups from prepared whole-source bounds, without fine geometry."""

import argparse
import hashlib
import io
import json
from pathlib import Path

import numpy as np
from native_source_clip_controls import read as read_controls
from native_source_triangle_materials import DATA
from source_clip_demand import outside_xy
from source_cluster_bounds import frame
from source_domain_chunk_study import decode_raw


def sha(data):
    return hashlib.sha256(data).digest()


def run(args):
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
    for angle, view in sorted(views.items()):
        operands = frame(view, placements)
        selected = []
        for identity, bounds, normal in zip(
            prepared["source_ids"], prepared["bounds"], prepared["normal"], strict=True
        ):
            upper = (
                outside_xy(bounds, operands)
                if normal and operands is not None
                else None
            )
            if upper is None or not np.any(upper < 0):
                selected.append(int(identity))
        chunks = {records[i] for i in selected if i in records}
        received, found = 0, set()
        with (args.chunk / "groups.bin").open("rb") as stream:
            for offset, length in sorted(chunks):
                stream.seek(offset)
                blob = stream.read(length)
                if len(blob) != length or sha(blob) != hashes[offset, length]:
                    raise ValueError("Group incomplete or changed; retain originals")
                received += len(blob)
                found.update(map(int, decode_raw(blob)["source_ids"]))
        if not {i for i in selected if i in records}.issubset(found):
            raise ValueError("Incomplete selected group; retain originals")
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
                "metadata_index_plus_group_bytes": len(raw) + len(index_raw) + received,
            }
        )
    result = {
        "bounds_sha256": sha(raw).hex(),
        "bounds_file_bytes": len(raw),
        "index_file_bytes": len(index_raw),
        "cases": rows,
        "scope": "Fresh Python process reads prepared metadata, index and demanded groups without original/candidate geometry. Bounds and per-group hashes were generated offline from the complete source/candidate arrays. View-only; live World/revisions, recipe/fallback bytes, shadows, simulation, native raster, physical IO and device time are not covered. Integrity failure raises for whole-original fallback; caller integration is not implemented.",
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bounds", type=Path, required=True)
    parser.add_argument("--bounds-sha256", required=True)
    parser.add_argument("--chunk", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
