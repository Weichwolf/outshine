"""Compare repeated and incremental reads for one immutable source cohort.

View-only finite controls; no global World residency, shadows or device admission.
"""

import argparse
import hashlib
import io
import json
from pathlib import Path

import numpy as np
from source_domain_chunk_study import decode_raw


def sha(data):
    return hashlib.sha256(data).digest()


def run_case(rows, records, hashes, fallback_sha, args, *, retain):
    held, fallback = {}, None
    total, peak, result = 0, 0, []
    for row in rows:
        selected = set(row["selected_source_ids"])
        required = {records[i] for i in selected if i in records}
        original = selected - records.keys()
        if len(original) != row["selected_original_fallback_sources"]:
            raise ValueError("Complete whole-original fallback demand required")
        blobs, received, requests = {}, 0, 0
        with (args.chunk / "groups.bin").open("rb") as stream:
            for offset, length in sorted(required):
                key = offset, length
                blob = held.get(key) if retain else None
                if blob is None:
                    stream.seek(offset)
                    blob = stream.read(length)
                    if len(blob) != length or sha(blob) != hashes[key]:
                        raise ValueError("Changed required group retains original")
                    received += len(blob)
                    requests += 1
                    if retain:
                        held[key] = blob
                blobs[key] = blob
        found = set()
        for blob in blobs.values():
            found.update(map(int, decode_raw(blob)["source_ids"]))
        if not (selected & records.keys()).issubset(found):
            raise ValueError("Every requested source must remain in its whole group")
        fallback_blob = fallback if retain else None
        if original and fallback_blob is None:
            fallback_blob = args.fallback.read_bytes()
            if sha(fallback_blob) != fallback_sha:
                raise ValueError("Changed original fallback")
            received += len(fallback_blob)
            requests += 1
            if retain:
                fallback = fallback_blob
        if original:
            with np.load(io.BytesIO(fallback_blob), allow_pickle=False) as z:
                if not original.issubset(
                    set(map(int, z["actual_triangle_binding"][:, 1]))
                ):
                    raise ValueError("Whole original fallback unavailable")
        resident = sum(map(len, held.values())) + (len(fallback) if fallback else 0)
        if resident > args.max_payload_bytes:
            raise ValueError("Cohort retention capacity exceeded; retain original")
        total += received
        peak = max(peak, resident)
        result.append(
            {
                "angle_degrees": row["angle_degrees"],
                "needed_sources": len(selected),
                "needed_groups": len(required),
                "needed_fallbacks": len(original),
                "payload_bytes_read": received,
                "payload_read_calls": requests,
                "retained_payload_bytes": resident,
            }
        )
    return {
        "payload_bytes_read": total,
        "peak_retained_blob_payload_bytes": peak,
        "steps": result,
    }


def run(args):
    view_raw, bounds_raw = args.view.read_bytes(), args.bounds.read_bytes()
    view = json.loads(view_raw)
    if sha(bounds_raw).hex() != view["bounds_sha256"]:
        raise ValueError("Recorded view/source metadata mismatch")
    with np.load(io.BytesIO(bounds_raw), allow_pickle=False) as z:
        index_sha = z["index_digest"].tobytes()
        fallback_sha = z["fallback_digest"].tobytes()
        hashes = {
            (int(o), int(n)): h.tobytes()
            for o, n, h in zip(
                z["group_offsets"], z["group_lengths"], z["group_digests"], strict=True
            )
        }
    index_raw = (args.chunk / "index.npy").read_bytes()
    if sha(index_raw) != index_sha:
        raise ValueError("Changed source index retains original")
    index = np.load(io.BytesIO(index_raw), allow_pickle=False)
    records = {
        int(row["source"]): (int(row["offset"]), int(row["bytes"])) for row in index
    }
    if len(records) != len(index) or set(records.values()) != set(hashes):
        raise ValueError("Complete source/group association required")
    by_angle = {row["angle_degrees"]: row for row in view["cases"]}
    angles = (90, 65, 60, 30, 0, 330, 300, 270, 240, 210, 180, 150, 120, 90)
    rows = [by_angle[angle] for angle in angles]
    baseline = run_case(rows, records, hashes, fallback_sha, args, retain=False)
    retained = run_case(rows, records, hashes, fallback_sha, args, retain=True)
    if any(
        (a["needed_sources"], a["needed_groups"], a["needed_fallbacks"])
        != (b["needed_sources"], b["needed_groups"], b["needed_fallbacks"])
        for a, b in zip(baseline["steps"], retained["steps"], strict=True)
    ):
        raise ValueError("Identical source and fallback demand required")
    report = {
        "bounds_sha256": sha(bounds_raw).hex(),
        "view_sha256": sha(view_raw).hex(),
        "cohort_capacity_bytes": args.max_payload_bytes,
        "fixed_view_bounds_and_index_bytes": len(view_raw)
        + len(bounds_raw)
        + len(index_raw),
        "baseline": baseline,
        "retained": retained,
        "scope": "Recorded rotations of one immutable cohort with previously computed view-only demand. Actual file-read return bytes, not physical IO, runtime frame time or native device. Group blobs and one verified original fallback are retained until cohort retirement; no cross-cohort policy, evictions, position changes, live revisions, recipes, shadow/simulation needs or all-world 360-degree readiness are proven. Index, bounds and view metadata remain resident and are counted separately; decoded objects, Python/OS overhead, copies, allocation traffic and peak RSS are not measured.",
    }
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "baseline_payload_bytes": baseline["payload_bytes_read"],
                "incremental_payload_bytes": retained["payload_bytes_read"],
                "peak_retained_payload_bytes": retained[
                    "peak_retained_blob_payload_bytes"
                ],
                "fixed_view_bounds_and_index_bytes": report[
                    "fixed_view_bounds_and_index_bytes"
                ],
                "steps": retained["steps"],
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("view", "bounds", "chunk", "fallback", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--max-payload-bytes", type=int, required=True)
    run(parser.parse_args())
