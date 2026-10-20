"""Export captured pre-mesh plan points for the native packing cost experiment."""

import argparse
import json
import tempfile
from pathlib import Path

import native_road_parts as corpus


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=corpus.DEFAULT_INPUT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if Path(tempfile.gettempdir()).resolve() not in output.parents:
        parser.error("diagnostic output must be inside the system temporary directory")
    # Existing whole-corpus verification is paid offline fixture preparation.
    # The C++ grouping experiment receives no fine mesh payload or bounds.
    manifest, _, parts, _, _ = corpus.load(args.input)
    plan = []
    for part in parts:
        edge = part["kind"] == "edge"
        points = part["planned_stations"] if edge else part["gates"]
        plan.append(
            {
                "kind": 0 if edge else 1,
                "index": part["edge"] if edge else part["junction"],
                "points": [row[:2] for row in points],
            }
        )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(
            {"archive_sha256": manifest["archive_sha256"], "parts": plan},
            separators=(",", ":"),
        )
    )


if __name__ == "__main__":
    main()
