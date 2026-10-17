"""Equal real selected regions: general union, coverage union or directed chains."""

import argparse
import contextlib
import hashlib
import json
import resource
import time
from pathlib import Path

import shapely
from camera_height_inputs import digest
from stair_composition_boundary import GENERAL_UNION, equal_coverage, oriented_equal
from stair_composition_scope import composition
from stair_module_experiment import write_json
from stair_patch_inputs import load_prepared
from stair_terrain_experiment import frozen


def inputs(products):
    report = json.loads((products / "comparison.json").read_text())
    receipt = json.loads((products / "verification.json").read_text())
    if not receipt.get("valid_prepared_snapshot") or receipt[
        "products_report_sha256"
    ] != digest(products / "comparison.json"):
        raise ValueError("independently verified actual complete snapshot required")
    if digest(products / "resident.json") != report["artifacts"]["resident.json"]:
        raise ValueError("actual snapshot manifest changed")
    resident = json.loads((products / "resident.json").read_text())
    if (
        Path(resident["terrain"]).resolve() != (products / "terrain").resolve()
        or digest(products / "terrain/patches.json") != resident["terrain_sha256"]
    ):
        raise ValueError("original prepared terrain binding changed")
    pairs = []
    with composition("general", [], captured=pairs):
        view, manifest = load_prepared(products / "terrain")
    if (
        manifest["frame"] != resident["frame"]
        or manifest["source_ids"] != resident["source_ids"]
    ):
        raise ValueError("original prepared sources or frame changed")
    if len(pairs) != len(view.patches) - 1:
        raise ValueError("one original region pair per actual replacement required")
    sha = hashlib.sha256()
    for before, after in pairs:
        for polygons in (before, after):
            if not shapely.coverage_is_valid(polygons):
                raise ValueError(
                    "actual selected regions violate coverage preconditions"
                )
            sha.update(str(len(polygons)).encode())
            sha.update(b"".join(shapely.to_wkb(polygons)))
        if not equal_coverage(before, after, "general"):
            raise ValueError("actual whole-XYZ-boundary replacement changed coverage")
    return pairs, manifest, sha.hexdigest()


def run(args):
    commit = frozen()
    for target in (args.output, args.profile):
        if target is not None and (
            target.exists() or not target.resolve().is_relative_to(Path("/tmp"))
        ):
            raise ValueError("fresh explicit private comparison outputs required")
    pairs, manifest, input_sha = inputs(args.products)
    tracker = contextlib.nullcontext()
    if args.profile:
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=False
        )
    with tracker:
        began = time.perf_counter()
        results = [equal_coverage(a, b, args.method) for a, b in pairs]
        seconds = time.perf_counter() - began
    if not all(results):
        raise ValueError("alternative changed actual region equality")
    proof_count = sum(oriented_equal(a, b) for a, b in pairs)
    if args.method == "coverage":
        for before, after in pairs:
            for polygons in (before, after):
                if not GENERAL_UNION(polygons).equals(
                    shapely.coverage_union_all(polygons)
                ):
                    raise ValueError("coverage union changed actual region topology")
    result = {
        "commit": commit,
        "method": args.method,
        "input_pairs_sha256": input_sha,
        "products_manifest_sha256": digest(args.products / "resident.json"),
        "frame": manifest["frame"],
        "pairs": len(pairs),
        "input_triangles": sum(len(a) + len(b) for a, b in pairs),
        "oriented_proofs": proof_count,
        "same_predicates": True,
        "profiled_seconds": seconds,
        "profiled": bool(args.profile),
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "scope": "Only existing selected-coverage equality operations after actual strict "
        "patch construction and whole XYZ boundary checks. Inputs/imports/independent coverage "
        "preconditions and topology controls excluded from measurement. Memray allocation "
        "traffic is not copied bytes; no new full-loader/runtime/device admission.",
    }
    write_json(args.output, result)
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", type=Path)
    parser.add_argument(
        "--method", choices=("general", "coverage", "oriented"), required=True
    )
    run(parser.parse_args())
