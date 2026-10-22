"""Compare geometric shadow-update work using captured native world matrices."""

import argparse
import gzip
import json
import math
import time
import tracemalloc
from pathlib import Path

FIXTURE = (
    Path(__file__).resolve().parents[2]
    / "outshine/data/render/regional-shadow-translation/frame-inputs.json.gz"
)


def overlap(previous, current, side):
    if len(previous) != 16 or len(current) != 16 or side <= 0:
        raise ValueError("invalid projection or tile size")
    if not all(math.isfinite(x) for matrix in (previous, current) for x in matrix):
        raise ValueError("non-finite native matrix")
    if previous == current:
        return None
    full = side * side
    reusable = all(previous[i] == current[i] for i in range(16) if i not in (12, 13))
    shift = [
        (current[i] - previous[i]) * side * sign / 2 for i, sign in ((12, 1), (13, -1))
    ]
    if not reusable or not all(x.is_integer() for x in shift):
        return {"retained": 0, "exposed": full, "copy_payload_bytes": 0, "shift": None}
    dx, dy = (int(x) for x in shift)
    retained = max(0, side - abs(dx)) * max(0, side - abs(dy))
    return {
        "retained": retained,
        "exposed": full - retained,
        "copy_payload_bytes": 2 * retained * 4,
        "shift": [dx, dy],
    }


def compare(fixture):
    frames, observed = fixture["frames"], fixture["observed"]
    if fixture["version"] != 1 or fixture["depth_bytes_per_texel"] != 4:
        raise ValueError("unsupported native capture")
    if len(frames) != len(observed) + 1 or not observed:
        raise ValueError("capture must include the preceding submitted state")
    side = fixture["tile_px"]
    rows = []
    for index, (before, after, actual) in enumerate(zip(frames, frames[1:], observed)):
        old, new = before["world_matrices"], after["world_matrices"]
        if len(old) != 4 or len(new) != 4:
            raise ValueError("capture must retain all four regions")
        changes = []
        mask = 0
        for region, (a, b) in enumerate(zip(old, new)):
            plan = overlap(a, b, side)
            if plan is not None:
                mask |= 1 << region
                changes.append(dict(region=region, **plan))
        if mask != actual["mask"]:
            raise ValueError(
                f"native matrix and independently observed packet masks differ at {index}"
            )
        rows.append({"frame": index, "mask": mask, "updates": changes})
    updates = [update for row in rows for update in row["updates"]]
    full = len(updates) * side * side
    exposed = sum(update["exposed"] for update in updates)
    return {
        "frames": len(observed),
        "region_updates": len(updates),
        "original_shadow_packets": sum(row["shadow_packets"] for row in observed),
        "full_redraw_clear_texels": full,
        "incremental_exposed_texels": exposed,
        "overlap_texels": full - exposed,
        "scratch_copy_payload_bytes": sum(
            update["copy_payload_bytes"] for update in updates
        ),
        "scratch_texture_payload_bytes": side * side * 4,
        "ring_overlap_copy_payload_bytes": 0,
        "incremental_shadow_packets": None,
        "gpu_times_ms": None,
        "gpu_allocation_bytes": None,
        "rows": rows,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=FIXTURE)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    started, cpu = time.perf_counter(), time.process_time()
    tracemalloc.start()
    packed = args.input.read_bytes()
    unpacked = gzip.decompress(packed)
    report = compare(json.loads(unpacked))
    report["model_diagnostic"] = {
        "wall_ms": (time.perf_counter() - started) * 1000,
        "cpu_ms": (time.process_time() - cpu) * 1000,
        "tracemalloc_peak_bytes": tracemalloc.get_traced_memory()[1],
        "input_compressed_bytes": len(packed),
        "input_json_bytes": len(unpacked),
        "allocation_count": None,
        "native_runtime_cost": False,
    }
    tracemalloc.stop()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    print(json.dumps({key: value for key, value in report.items() if key != "rows"}))


if __name__ == "__main__":
    main()
