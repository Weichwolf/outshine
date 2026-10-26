"""Explicit isolated negative SFCGAL intersection probe; never a runtime dependency."""

import argparse
import json
import subprocess
import sys
from pathlib import Path

import shapely
from stair_terrain_experiment import frozen

CHILD = """import json, resource, sys
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
resource.setrlimit(resource.RLIMIT_CPU, (3, 3))
resource.setrlimit(resource.RLIMIT_AS, (1024**3, 1024**3))
from pysfcgal.geometry import Geometry
a = Geometry.from_wkb(sys.stdin.read().strip())
b = Geometry.from_wkb(a.to_wkb())
valid = bool(a.is_valid() and b.is_valid())
print(json.dumps({"source_valid": valid}), flush=True)
if not valid:
    raise ValueError("native source conversion/validity failed")
result = a.intersection(b)
if result is None:
    raise ValueError("native intersection returned no object")
print(json.dumps({"result_type": result.geom_type, "exact_wkt": result.to_wkt()}), flush=True)
"""


def run(args):
    import pysfcgal

    commit = frozen()
    if (
        not args.output.resolve().is_relative_to(Path("/tmp").resolve())
        or args.output.exists()
    ):
        raise ValueError("fresh explicit /tmp native-limit receipt required")
    source = shapely.box(0, 0, 1e-170, 1e-170)
    result = subprocess.run(
        [sys.executable, "-X", "faulthandler", "-c", CHILD],
        input=source.wkb_hex,
        text=True,
        capture_output=True,
        timeout=6,
        check=False,
    )
    receipt = {
        "commit": commit,
        "sfcgal": pysfcgal.sfcgal_full_version(),
        "input_wkb": source.wkb_hex,
        "child_code": CHILD,
        "returncode": result.returncode,
        "stdout": result.stdout,
        "stderr": result.stderr,
        "limits": {
            "cpu_seconds": 3,
            "address_space_bytes": 1024**3,
            "wall_seconds": 6,
            "core_bytes": 0,
        },
        "scope": "Native failure is an oracle limitation, not evidence of an empty conflict. Exact axis-aligned area controls are independent of this boolean operation.",
    }
    args.output.write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps({"returncode": result.returncode, "receipt": str(args.output)}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
