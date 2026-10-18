"""Fresh-process same-content work/timing and separate Memray allocation evidence."""

import argparse
import contextlib
import hashlib
import json
import resource
import time
from pathlib import Path

from bark_furrows import FurrowRecipe
from furrow_raster import raster


def run(output, method, profile):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private profiling output must be under /tmp")
    output.mkdir(parents=True, exist_ok=False)
    context = contextlib.nullcontext()
    if profile:
        import memray

        context = memray.Tracker(
            output / "allocations.bin", trace_python_allocators=True
        )
    start = time.perf_counter()
    with context:
        field, work = raster(FurrowRecipe(), method=method)
    elapsed = time.perf_counter() - start
    report = {
        "method": method,
        "profile": profile,
        "seconds": elapsed,
        **work,
        "field_sha256": hashlib.sha256(field.tobytes()).hexdigest(),
        "output_array_bytes": field.nbytes,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Complete 1024x2048 (width x height) same furrow field. Module-level imports/PNG/map shading excluded; first-use internal imports included. Digest copy after tracked region; RSS includes imports and digest. Profile times are not compared with uninstrumented times.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--method", choices=("dense", "indexed"), required=True)
    parser.add_argument("--profile", action="store_true")
    args = parser.parse_args()
    run(args.output, args.method, args.profile)
