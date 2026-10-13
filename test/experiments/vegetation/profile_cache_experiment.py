"""Fresh-process cached-profile loading without curve preparation or SciPy imports."""

import argparse
import hashlib
import importlib.util
import json
import resource
import subprocess
import sys
import time
from pathlib import Path


def child(mode, root):
    start = time.perf_counter()
    if mode == "before":
        spec = importlib.util.spec_from_file_location(
            "cache_profile_before", root / "profile_curve.before.py"
        )
        module = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = module
        spec.loader.exec_module(module)
        profile_type = module.Profile
    else:
        sys.modules["scipy"] = None
        from outshine_ref.vegetation import Profile

        profile_type = Profile
    imported = time.perf_counter()
    import numpy as np

    with np.load(root / "current/linear-profile.npz") as saved:
        profile = profile_type(saved["table"])
    loaded = time.perf_counter()
    actual = profile.evaluate(np.linspace(-2, 3, 2048), tiled=True)
    done = time.perf_counter()
    return {
        "mode": mode,
        "import_s": imported - start,
        "load_s": loaded - imported,
        "query_s": done - loaded,
        "peak_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "scipy_loaded": any(
            k.startswith("scipy") and v is not None for k, v in sys.modules.items()
        ),
        "output_sha256": hashlib.sha256(actual.tobytes()).hexdigest(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("before", "after"))
    parser.add_argument(
        "--root",
        type=Path,
        default=Path("build/experiments/vegetation/profile-library"),
    )
    args = parser.parse_args()
    if args.mode:
        print(json.dumps(child(args.mode, args.root)))
        return
    records = []
    for repeat in range(5):
        for mode in ("before", "after") if repeat % 2 == 0 else ("after", "before"):
            result = subprocess.run(
                [sys.executable, __file__, "--mode", mode, "--root", str(args.root)],
                text=True,
                capture_output=True,
                timeout=30,
                check=True,
            )
            record = json.loads(result.stdout)
            assert record["scipy_loaded"] == (mode == "before")
            records.append(record)
    assert len({record["output_sha256"] for record in records}) == 1
    report = {
        "fresh_processes": records,
        "cached_outputs_exact": True,
        "curve_construction_required": False,
    }
    (args.root / "cache.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
