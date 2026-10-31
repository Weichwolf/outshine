"""Pinned offline control counts for demand filtering before group/recipe IO.

Counts cover ring occurrences only, not GEOS time or native admission.
"""

import argparse
import hashlib
import io
from pathlib import Path

import numpy as np
from source_domain_buffer import control_count
from source_domain_chunk_study import decode_raw


def sha(raw):
    return hashlib.sha256(raw).digest()


def prepare(bounds, directory, output):
    if output.exists():
        raise ValueError("Fresh control index output required")
    bound_raw = bounds.read_bytes()
    with np.load(io.BytesIO(bound_raw), allow_pickle=False) as z:
        expected = z["index_digest"].tobytes()
        hashes = {
            (int(o), int(n)): h.tobytes()
            for o, n, h in zip(
                z["group_offsets"], z["group_lengths"], z["group_digests"], strict=True
            )
        }
    raw = (directory / "index.npy").read_bytes()
    if sha(raw) != expected:
        raise ValueError("Same prepared source index required")
    index = np.load(io.BytesIO(raw), allow_pickle=False)
    counts = {}
    with (directory / "groups.bin").open("rb") as stream:
        for (offset, size), expected_hash in hashes.items():
            stream.seek(offset)
            blob = stream.read(size)
            if len(blob) != size or sha(blob) != expected_hash:
                raise ValueError("Whole bound group required")
            group = decode_raw(blob)
            for source, identity in enumerate(group["source_ids"]):
                identity = int(identity)
                if identity in counts:
                    raise ValueError("Unique complete source counts required")
                counts[identity] = control_count(group, source)
    if set(counts) != set(map(int, index["source"])):
        raise ValueError("Complete source counts required")
    np.savez(
        output,
        controls=np.array([counts[int(i)] for i in index["source"]], dtype="<u4"),
        index_digest=np.frombuffer(expected, dtype="u1"),
        bounds_digest=np.frombuffer(sha(bound_raw), dtype="u1"),
    )


def load(path, expected_sha, index_raw, bound_raw):
    raw = path.read_bytes()
    if sha(raw).hex() != expected_sha:
        raise ValueError("Pinned control index required")
    with np.load(io.BytesIO(raw), allow_pickle=False) as z:
        counts = z["controls"]
        index = np.load(io.BytesIO(index_raw), allow_pickle=False)
        if (
            z["index_digest"].tobytes() != sha(index_raw)
            or z["bounds_digest"].tobytes() != sha(bound_raw)
            or counts.dtype != np.dtype("<u4")
            or counts.shape != (len(index),)
            or np.any(counts < 3)
            or np.any(counts > 1048576)
        ):
            raise ValueError("Complete bound control counts required")
    return dict(zip(map(int, index["source"]), map(int, counts), strict=True)), len(raw)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("bounds", "chunk", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    prepare(args.bounds, args.chunk, args.output)
