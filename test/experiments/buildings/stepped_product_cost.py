"""Equal physical product: rebuild after validation versus retain the successful product."""

import argparse
import hashlib
import json
import resource
import time
from pathlib import Path
from unittest.mock import patch

import stepped_source
from evidence import revision
from stepped_profile_probe import sources


def fingerprint(mesh):
    digest = hashlib.sha256()
    for key in sorted(mesh):
        value = mesh[key]
        digest.update(key.encode())
        digest.update(str((value.dtype.str, value.shape)).encode())
        digest.update(value.tobytes())
    return digest.hexdigest()


def run(index, method, repeats):
    source = sources()[index]
    profile = "corner" if index == 0 else "diagonal"
    original = stepped_source.near
    records, expected = [], None
    for repeat in range(repeats + 1):
        calls, returned_bytes = 0, 0

        def counted(*args):
            nonlocal calls, returned_bytes
            calls += 1
            product = original(*args)
            returned_bytes += sum(a.nbytes for a in product[0].values())
            return product

        start = time.perf_counter()
        with patch("stepped_source.near", side_effect=counted):
            if method == "rebuild":
                plan = stepped_source.plan_for(source, profile)
                product = stepped_source.near(plan, 1)
            else:
                plan, product = stepped_source.plan_and_near(source, profile)
        elapsed = time.perf_counter() - start
        actual = fingerprint(product[0])
        if expected is not None and expected != actual:
            raise ValueError("Repeated physical product buffers changed")
        expected = actual
        if repeat:
            records.append(
                {
                    "seconds": elapsed,
                    "near_calls": calls,
                    "successful_near_result_array_bytes": returned_bytes,
                }
            )
    return revision() | {
        "source_id": source["id"],
        "method": method,
        "runs": records,
        "physical_mesh_sha256": expected,
        "physical_triangles": len(product[0]["faces"]),
        "physical_result_arrays": len(product[0]),
        "physical_result_bytes": sum(a.nbytes for a in product[0].values()),
        "room_ids": plan["storey_room_ids"],
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "One process per method/source, one warmup omitted. Normal-priority nonisolated CPU measure alongside nice10 renders/checks. Successful owned near-result arrays counted; failed/native/temporary allocations and all internal copies not enumerated. Not a frame/device measurement.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", type=int, choices=(0, 2), required=True)
    parser.add_argument("--method", choices=("rebuild", "reuse"), required=True)
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.repeats <= 5:
        parser.error("One to five measured repetitions required")
    args.output.write_text(
        json.dumps(run(args.case, args.method, args.repeats), indent=2) + "\n"
    )
