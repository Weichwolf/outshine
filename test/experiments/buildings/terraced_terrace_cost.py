"""Incremental whole-owner costs for protected terraces; different-quality context."""

import argparse
import gc
import json
import resource
import time
from pathlib import Path

from evidence import revision
from terraced_facade_prepare import prepare
from terraced_facade_product_cost import payload
from terraced_facade_products import FacadeProducts
from terraced_mass_plan import plan as mass_plan
from terraced_mass_sources import sources
from terraced_terrace_geometry import TerraceProducts
from terraced_terrace_plan import plan


def arrays(value, near):
    owner = value.facade if isinstance(value, TerraceProducts) else value
    mesh = owner.coarse
    result = payload(mesh)
    if near:
        p = payload(owner.physical())
        result = {k: result[k] + p[k] for k in result}
    if isinstance(value, TerraceProducts):
        extra = [a for c in value.components for a in c.mesh.values()]
        extra += [
            a for mesh in (*value.additions, value.portal_frames) for a in mesh.values()
        ]
        unique = {(int(a.__array_interface__["data"][0]), a.nbytes): a for a in extra}
        result["output_numpy_bytes"] += sum(a.nbytes for a in unique.values())
        result["output_numpy_buffers"] += len(unique)
        result["triangles"] += sum(
            len(m["faces"]) for m in (*value.additions, value.portal_frames)
        )
    return result


def run(args):
    mass = mass_plan(sources()[args.source], args.programme)
    records = []
    for current in (False, True):

        def build(current=current):
            value = (
                TerraceProducts(plan(mass))
                if current
                else FacadeProducts(prepare(mass))
            )
            owner = value.facade if current else value
            if args.near:
                owner.physical()
            return value

        timings, value = [], None
        for _ in range(3):
            del value
            gc.collect()
            cpu, start = time.process_time(), time.perf_counter()
            value = build()
            timings.append(
                {
                    "wall_s": time.perf_counter() - start,
                    "cpu_s": time.process_time() - cpu,
                }
            )
        result = arrays(value, args.near)
        plan_counts = (
            {
                "terraces": len(value.plan.terraces),
                "portals": len(value.plan.portals),
                "components": len(value.components),
            }
            if current
            else {}
        )
        del value
        gc.collect()
        trace = args.output.with_name(
            args.output.stem + ("-after.bin" if current else "-before.bin")
        )
        import memray

        with memray.Tracker(trace, trace_python_allocators=True, native_traces=False):
            traced = build()
        if arrays(traced, args.near) != result:
            raise ValueError("Profile replay changed retained payload")
        records.append(
            {
                "current": current,
                "unprofiled_runs": timings,
                "trace": str(trace),
                **result,
                **plan_counts,
            }
        )
        del traced
    return revision() | {
        "source": args.source,
        "programme": args.programme,
        "near": args.near,
        "records": records,
        "worker_peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "Three unprofiled fresh-owner constructions per variant in one fresh worker; mass/imports/source acquisition excluded. Planning, direct product and optional whole physical product included. Current has additional deck/guard/portal quality: no equal-quality speed claim. All stored mesh arrays retained; source WKB/IDs/Python object heap outside payload. Mesh payload counts include separately retained collision/raw and render arrays. Memray traces separate builds including Python/native requests, no native stacks; cumulative traffic and tracked heap are not RSS or total copies. No native/frame/device budget admission.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=int, choices=range(4), required=True)
    parser.add_argument(
        "--programme", choices=("cascade", "paired-wings"), required=True
    )
    parser.add_argument("--near", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("--output requires a file under /tmp")
    if revision()["experiment_tree_dirty"]:
        parser.error("Freeze the experiment before measuring")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    result = run(args)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "source": args.source,
                "programme": args.programme,
                "near": args.near,
                "records": result["records"],
            }
        )
    )


if __name__ == "__main__":
    main()
