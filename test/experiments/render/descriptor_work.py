"""Compare allocation work on an actual native binding trace, without GPU timing claims."""

import argparse
import hashlib
import json
from pathlib import Path


def key(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def measure(requests):
    whole = {}
    sets = set()
    uniforms = set()
    requested_sets = 0
    uniform_requests = 0
    uniform_bytes = 0
    for row in requests:
        requested_sets += len(row["sets"])
        whole[(tuple(row["sets"]), key(row["bindings"]))] = len(row["sets"])
        for index, layout in enumerate(row["sets"]):
            values = [value for value in row["bindings"] if value["set"] == index]
            sets.add((layout, key(values)))
        for value in row["bindings"]:
            if value["kind"] == 3:
                uniform_requests += 1
                uniform_bytes += len(bytes.fromhex(value["bytes"]))
                uniforms.add(value["bytes"])
    return {
        "requests": len(requests),
        "allocate_every_request": {
            "sets": requested_sets,
            "uniform_buffer_allocations": uniform_requests,
            "uniform_source_copy_bytes": uniform_bytes,
        },
        "whole_binding_reuse": {"sets": sum(whole.values()), "states": len(whole)},
        "per_set_reuse": {"sets": len(sets)},
        "uniform_snapshot_reuse": {
            "values": len(uniforms),
            "unique_bytes": sum(len(bytes.fromhex(value)) for value in uniforms),
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    if args.output.resolve().is_relative_to(root):
        parser.error("private results belong in the system temporary directory")
    raw = args.input.read_bytes()
    trace = json.loads(raw)
    requests = trace["requests"]
    result = {
        "input_sha256": hashlib.sha256(raw).hexdigest(),
        "provenance": trace["provenance"],
        "accepted_prefix_without_final_rejection": measure(requests[:-1]),
        "requested_prefix_including_rejection": measure(requests),
        "limits": "Logical allocation/copy work only; no driver memory, latency or frame proof.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result["requested_prefix_including_rejection"], sort_keys=True))


if __name__ == "__main__":
    main()
