"""Equal-input contact range versus the previous two-direction surface traversal."""

import argparse
import resource
from pathlib import Path
from unittest.mock import patch

import stair_support_audit
from outshine_ref.geometry import vertical_clearance
from stair_local_cost import run
from stair_module_experiment import write_json


def two_directions(first, second):
    """Independent unchanged pre-#706 contact calculation, for comparisons only."""
    values, checks = [], 0
    for low, high in ((first, second), (second, first)):
        note = vertical_clearance(low, high, max_faces=4096, max_pairs=65536, seconds=2)
        if not note["valid"]:
            raise ValueError("pier whole contact unresolved: " + note["reason"])
        values.append(abs(note["minimum_clearance_m"]))
        checks += note["checks"]
    return max(values), checks


def measure(args):
    records = []
    implementation = (
        two_directions
        if args.variant == "two-directions"
        else stair_support_audit.contact_error
    )

    def observe(first, second):
        error, checks = implementation(first, second)
        records.append([error, checks])
        return error, checks

    with patch.object(stair_support_audit, "contact_error", observe):
        result = run(args)
    result["contact_comparison"] = {
        "variant": args.variant,
        "contact_error_calls": len(records),
        "signed_surface_traversals": len(records)
        * (2 if args.variant == "two-directions" else 1),
        "candidate_pair_checks": sum(row[1] for row in records),
        "errors_and_pair_checks": records,
        "scope": "Observed contact adapter only; whole construct measured by stair_local_cost. The same wrapper/records are included in both variants. Arrays hashed after timing/tracing. No runtime or device acceptance.",
    }
    write_json(args.output, result)
    return result


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("resident", "baseline", "fixture", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    parser.add_argument("--variant", choices=("two-directions", "range"), required=True)
    parser.add_argument("--profile", type=Path)
    parser.set_defaults(mode="local")
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    measure(arguments())
