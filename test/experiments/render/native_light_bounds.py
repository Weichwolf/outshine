"""Check the V1 light lookup against a permitted division-rounding countermodel."""

import argparse
import itertools
import json
import subprocess
from pathlib import Path

import numpy as np


def fixture():
    lights = np.zeros((18, 4, 4), dtype="<f4")
    lights[:, 0, 3] = 1
    lights[:, 1, 0] = np.arange(1, 19) * 128
    lights[:, 1, 1] = 2
    lights[:, 1, 3] = 0.125
    lights[:, 2, 1] = -1
    lights[:, 3, 0] = 0.5
    lights[:, 3, 1] = 2
    lights[-1, 0, :3] = 1
    lights[-1, 1, 0] = 0
    return lights


def cells(words):
    result = {}
    for slot in range(int(words[3]) + 1):
        at = int(words[2]) + 5 * slot
        if words[at + 4] == 0:
            continue
        key = [int(value) ^ 0x80000000 for value in words[at : at + 3]]
        key = tuple(value if value < 2**31 else value - 2**32 for value in key)
        offset, count = map(int, words[at + 3 : at + 5])
        result[key] = set(map(int, words[offset : offset + count]))
    return result


def selected(words, origin, ratio):
    position = np.floor(origin / 128).astype(np.int64)
    local_origin = (origin - position * 128).astype(np.float32)
    lookup = np.floor(origin / 32).astype(np.int64)
    carry = lookup - position * ratio
    local = (local_origin - carry.astype(np.float32) * np.float32(32)).astype(
        np.float32
    )
    maximum = words[5:6].view("<f4")[0]
    margin = np.float32(16) * np.float32(2**-23) * (maximum + np.float32(128))
    first = lookup + np.floor((local - margin) * np.float32(1 / 32)).astype(np.int64)
    last = lookup + np.floor((local + margin) * np.float32(1 / 32)).astype(np.int64)
    if np.any(first < -(2**31)) or np.any(last >= 2**31) or np.any(last - first > 1):
        return set(range(int(words[6])))
    table = cells(words)
    result = set(map(int, words[int(words[0]) : int(words[0] + words[1])]))
    for key in itertools.product(*(range(a, b + 1) for a, b in zip(first, last))):
        result.update(table.get(key, ()))
    return result


def check(binary, output, name, origin):
    prefix = output / name
    source = prefix.with_suffix(".source")
    source.write_bytes(fixture().tobytes())
    paths = [
        prefix.with_suffix(suffix) for suffix in (".words", ".lights", ".positions")
    ]
    command = [str(binary), str(source), *map(str, paths), *map(str, origin)]
    run = subprocess.run(
        command, text=True, capture_output=True, timeout=60, check=True
    )
    prefix.with_suffix(".log").write_text(run.stdout + run.stderr)
    words = np.fromfile(paths[0], dtype="<u4")
    if words[6] != 18 or json.loads(run.stdout)["full"]:
        raise AssertionError(
            "the native producer must retain a finite eighteen-source hash"
        )
    quotient = np.float32(4)
    below = np.nextafter(quotient, np.float32(0))
    exact = selected(words, origin, int(quotient))
    permitted = selected(words, origin, int(below))
    fixed = selected(words, origin, 4)
    if 17 not in exact or 17 not in fixed or (name != "local" and 17 in permitted):
        raise AssertionError(
            "expected native-ID counterexample/control did not reproduce"
        )
    return {
        "case": name,
        "command": command,
        "producer_exit": run.returncode,
        "exact_division_id17": 17 in exact,
        "permitted_division_id17": 17 in permitted,
        "fixed_integer_ratio_id17": 17 in fixed,
        "quotient_exact": float(quotient),
        "quotient_one_ulp_below": float(below),
        "integer_ratio_after_conversion": int(below),
        "positive_packed_range_attenuation": (1 - (4 * 0.125**2) ** 2) / 4,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("external output required")
    args.output.mkdir(parents=True, exist_ok=False)
    rows = []
    for name, origin in [
        ("local", [0, 0, 0]),
        ("earth", [6_000_000, -4_000_000, 3_200_000]),
        ("negative", [-6_000_000, 4_000_000, -3_200_000]),
    ]:
        rows.append(
            check(args.binary, args.output, name, np.array(origin, dtype=float))
        )
    report = {
        "scope": "Permitted-error arithmetic countermodel using actual native producer IDs; "
        "not an observed Iris shader failure, universal proof or performance result. "
        "OpFDiv may differ by2.5ULP; one representable value below4 truncates to3. "
        "The corrected V1 ratio is integer4; lookup uses the exact binary scale1/32.",
        "source": "Vulkan specification, spirvenv-op-prec / precision-core-table",
        "cases": rows,
    }
    (args.output / "receipt.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
