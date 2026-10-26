"""Compare the native projection path with a disk-support infeasibility witness."""

import argparse
import copy
import json
import math
import time
from decimal import Decimal, localcontext
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def capacity(case):
    for index, constraint in enumerate(case["constraints"]):
        maximum = sum(
            math.hypot(east, north) * case["slopes"][junction][2]
            for junction, east, north in constraint["terms"]
        )
        if constraint["minimum_rise_m"] > maximum + 1e-10:
            return {"constraint": index, "maximum_rise_m": maximum}
    return None


def legacy(case):
    slopes = copy.deepcopy(case["slopes"])
    active = sorted({t[0] for c in case["constraints"] for t in c["terms"]})
    counts = {"sweeps": 0, "term_dot_products": 0, "slope_norms": 0}

    def rise(constraint):
        counts["term_dot_products"] += len(constraint["terms"])
        return sum(
            east * slopes[junction][0] + north * slopes[junction][1]
            for junction, east, north in constraint["terms"]
        )

    for _ in range(10000):
        counts["sweeps"] += 1
        for constraint in case["constraints"]:
            missing = constraint["minimum_rise_m"] - rise(constraint)
            if missing <= 0:
                continue
            squared = sum(e * e + n * n for _, e, n in constraint["terms"])
            if not squared:
                return False, counts
            change = missing / squared
            for junction, east, north in constraint["terms"]:
                slopes[junction][0] += change * east
                slopes[junction][1] += change * north
        for junction in active:
            east, north, maximum = slopes[junction]
            length = math.hypot(east, north)
            counts["slope_norms"] += 1
            if length > maximum:
                slopes[junction][0] *= maximum / length
                slopes[junction][1] *= maximum / length
        if all(rise(c) >= c["minimum_rise_m"] - 1e-10 for c in case["constraints"]):
            return True, counts
    return False, counts


def oracle(case, constraint):
    with localcontext() as context:
        context.prec = 60
        total = Decimal(0)
        for junction, east, north in constraint["terms"]:
            e, n = Decimal.from_float(east), Decimal.from_float(north)
            radius = Decimal.from_float(case["slopes"][junction][2])
            total += (e * e + n * n).sqrt() * radius
        return total


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--case",
        type=Path,
        default=ROOT / "test/outshine/data/transport/wien-junction-slopes.json",
    )
    arguments = parser.parse_args()
    case = json.loads(arguments.case.read_text())
    began = time.perf_counter()
    fitted, counts = legacy(case)
    legacy_ms = (time.perf_counter() - began) * 1000
    began = time.perf_counter()
    witness = capacity(case)
    capacity_ms = (time.perf_counter() - began) * 1000
    if fitted or witness is None:
        raise ValueError("the fixed real case must retain its original infeasibility")
    constraint = case["constraints"][witness["constraint"]]
    exact = oracle(case, constraint)
    if Decimal.from_float(constraint["minimum_rise_m"]) <= exact:
        raise ValueError("the independent high-precision bound contradicts the witness")
    control = copy.deepcopy(case)
    control["constraints"][witness["constraint"]]["minimum_rise_m"] = 0.5 * float(exact)
    if capacity(control) is not None or not legacy(control)[0]:
        raise ValueError("the feasible control must remain solvable")
    result = {
        "case": str(arguments.case),
        "legacy_fitted": fitted,
        "legacy_work": counts,
        "witness": witness,
        "independent_maximum_rise_m": str(exact),
        "witness_slope_norms": len(constraint["terms"]),
        "legacy_ms": legacy_ms,
        "capacity_ms": capacity_ms,
        "scope": "Python diagnostic incl. work counters; no native/frame timing claim. "
        "A support bound proves this local request impossible; it does not repair its recipe.",
    }
    arguments.output.mkdir(parents=True, exist_ok=True)
    (arguments.output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
