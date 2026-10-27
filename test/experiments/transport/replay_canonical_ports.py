"""Replay captured native inputs with explicit cuts, methods and temporary output."""

import argparse
import json
import resource
import sys
import time
from collections.abc import Mapping
from dataclasses import fields, is_dataclass
from pathlib import Path

from canonical_port_native_inputs import FIXTURE, feature_spans, interior_demands, load
from outshine_ref.transport.canonical_ports import derive_canonical_ports


def serializable(value):
    if is_dataclass(value):
        return {
            field.name: serializable(getattr(value, field.name))
            for field in fields(value)
        }
    if isinstance(value, Mapping):
        return {str(key): serializable(item) for key, item in value.items()}
    if isinstance(value, (tuple, list)):
        return [serializable(item) for item in value]
    return value


def process_peak():
    status = Path("/proc/self/status")
    if status.exists():
        # Linux rusage can retain an ancestor's pre-exec maximum; VmHWM belongs to this image.
        peak = next(
            line
            for line in status.read_text().splitlines()
            if line.startswith("VmHWM:")
        )
        return int(peak.split()[1]) * 1024, "Linux /proc/self/status VmHWM"
    factor = 1 if sys.platform == "darwin" else 1024
    return resource.getrusage(
        resource.RUSAGE_SELF
    ).ru_maxrss * factor, "getrusage process maximum"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, default=FIXTURE)
    parser.add_argument(
        "--feature-ring", type=int, nargs=2, action="append", required=True
    )
    parser.add_argument("--level", type=int, default=20)
    parser.add_argument("--maximum-prior-change-m", type=float, required=True)
    parser.add_argument(
        "--method", choices=("auto", "affine", "cone", "lp"), default="auto"
    )
    parser.add_argument("--reverse", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("fresh explicit output required")
    source, terrain, terrain_binding, manifest = load(args.inputs)
    spans = tuple(
        span
        for feature, ring in args.feature_ring
        for span in feature_spans(source, feature, ring)
    )
    if not spans:
        parser.error("selected source references have no captured physical spans")
    demands = interior_demands(source, spans, args.level)
    if args.reverse:
        demands = demands[::-1]
    started, cpu = time.perf_counter(), time.process_time()
    result = derive_canonical_ports(
        source,
        demands,
        terrain,
        terrain_binding=terrain_binding,
        maximum_prior_change_m=args.maximum_prior_change_m,
        method=args.method,
    )
    elapsed, cpu = time.perf_counter() - started, time.process_time() - cpu
    peak, peak_source = process_peak()
    report = {
        "schema": "outshine/canonical-port-replay/1",
        "native_raw_sha256": manifest["native_raw_sha256"],
        "demands": demands,
        "values": result.values,
        "reasons": result.reasons,
        "report": result.report,
        "measurement": {
            "wall_ms": elapsed * 1000,
            "process_cpu_ms": cpu * 1000,
            "process_peak_rss_bytes": peak,
            "rss_source": peak_source,
            "rss_scope": "Whole process image including imports and decoded input, not incremental allocations",
            "scope": "Offline Python reference; no native or device budget admission",
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x") as stream:
        json.dump(serializable(report), stream, indent=2, allow_nan=False)
        stream.write("\n")
    print(
        json.dumps(
            {
                "output": str(args.output),
                "ports": len(demands),
                "resolved": sum(v is not None for v in result.values),
                **report["measurement"],
            }
        )
    )
    return 0 if all(value is not None for value in result.values) else 1


if __name__ == "__main__":
    raise SystemExit(main())
