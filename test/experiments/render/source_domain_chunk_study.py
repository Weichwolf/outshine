"""Experimental indexed source-group reads; not a production cache format."""

import argparse
import hashlib
import io
import json
from pathlib import Path

import numpy as np
from source_domain_buffer import control_at, domain_at


def subset(a, start, end):
    lo, hi = map(int, a["source_patch_offsets"][[start, end]])
    p, q = map(int, a["patch_polygon_offsets"][[lo, hi]])
    r, t = map(int, a["polygon_ring_offsets"][[p, q]])
    v, w = map(int, a["ring_vertex_offsets"][[r, t]])
    original = np.r_[a["local_frames"][lo:hi].ravel(), a["coordinates"][v:w].ravel()]
    words, inverse = np.unique(original, return_inverse=True)
    if len(words) > 65536:
        raise ValueError("Group needs wider indices")
    frame_count = (hi - lo) * 9
    info = a["patch_info"][lo:hi].copy()
    info[:, 0] -= start
    chunk = {
        "source_ids": a["source_ids"][start:end],
        "source_frames": a["source_frames"][start:end],
        "source_patch_offsets": a["source_patch_offsets"][start : end + 1] - lo,
        "patch_info": info,
        "patch_polygon_offsets": a["patch_polygon_offsets"][lo : hi + 1] - p,
        "polygon_ring_offsets": a["polygon_ring_offsets"][p : q + 1] - r,
        "ring_vertex_offsets": a["ring_vertex_offsets"][r : t + 1] - v,
        "float64_words": a["float64_words"][words],
        "local_frames": inverse[:frame_count].reshape(-1, 3, 3).astype("<u2"),
        "coordinates": inverse[frame_count:].reshape(-1, 2).astype("<u2"),
    }
    if not np.array_equal(
        chunk["float64_words"][inverse].view("u8"),
        a["float64_words"][original].view("u8"),
    ):
        raise ValueError("Every control word must survive grouping")
    return chunk


def run(source, output, group_size, identity):
    output.mkdir(parents=True, exist_ok=False)
    with np.load(source, allow_pickle=False) as z:
        a = {key: z[key] for key in z.files}
    n = len(a["source_ids"])
    index = np.empty(n, dtype=[("source", "<i4"), ("offset", "<u8"), ("bytes", "<u4")])
    groups = 0
    with (output / "groups.bin").open("wb") as stream:
        for start in range(0, n, group_size):
            end = min(n, start + group_size)
            chunk = subset(a, start, end)
            memory = io.BytesIO()
            np.savez(memory, **chunk)
            blob = memory.getvalue()
            index[start:end] = list(
                zip(
                    chunk["source_ids"],
                    [stream.tell()] * (end - start),
                    [len(blob)] * (end - start),
                    strict=True,
                )
            )
            stream.write(blob)
            groups += 1
    np.save(output / "index.npy", index, allow_pickle=False)
    loaded_index = np.load(output / "index.npy", allow_pickle=False)
    match = loaded_index[loaded_index["source"] == identity]
    if len(match) != 1:
        raise ValueError("One actual source identity required")
    with (output / "groups.bin").open("rb") as stream:
        stream.seek(int(match[0]["offset"]))
        blob = stream.read(int(match[0]["bytes"]))
    if len(blob) != int(match[0]["bytes"]):
        raise ValueError("Incomplete group retains original")
    with np.load(io.BytesIO(blob), allow_pickle=False) as z:
        loaded = {key: z[key] for key in z.files}
    old_source = int(np.flatnonzero(a["source_ids"] == identity)[0])
    new_source = int(np.flatnonzero(loaded["source_ids"] == identity)[0])
    old_lo, old_hi = map(int, a["source_patch_offsets"][old_source : old_source + 2])
    new_lo, new_hi = map(
        int, loaded["source_patch_offsets"][new_source : new_source + 2]
    )
    if old_hi - old_lo != new_hi - new_lo:
        raise ValueError("Whole source patch inventory required")
    for old, new in zip(range(old_lo, old_hi), range(new_lo, new_hi), strict=True):
        if (
            domain_at(a, old).wkb != domain_at(loaded, new).wkb
            or not np.array_equal(
                control_at(a, "local_frames", old).view("u8"),
                control_at(loaded, "local_frames", new).view("u8"),
            )
            or not np.array_equal(
                a["patch_info"][old, 1:], loaded["patch_info"][new, 1:]
            )
        ):
            raise ValueError("Source domain controls or topology changed")
    sizes = {
        name: (output / name).stat().st_size for name in ("index.npy", "groups.bin")
    }
    report = {
        "input_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "sources": n,
        "groups": groups,
        "group_size": group_size,
        "original_npz_bytes": source.stat().st_size,
        "indexed_file_bytes": sizes,
        "indexed_total_bytes": sum(sizes.values()),
        "sample_source": identity,
        "sample_patches_verified": new_hi - new_lo,
        "sample_index_plus_chunk_read_bytes": sizes["index.npy"] + len(blob),
        "sample_chunk_read_bytes": len(blob),
        "scope": "Experimental uncompressed NPZ blobs and in-memory index; bytes returned by application reads, not physical IO or native cache. Source recipe, original fallback, visibility, OS cache, time and peak memory are excluded. All group control words roundtrip; selected actual source polygon topology/labels also checked.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--group-size", type=int, default=8, choices=(1, 8, 32))
    parser.add_argument("--source", type=int, default=1147)
    args = parser.parse_args()
    run(args.input, args.output, args.group_size, args.source)
