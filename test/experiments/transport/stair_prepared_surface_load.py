"""Compare complete strict and externally admitted prepared-state loading."""

import argparse
import contextlib
import json
import resource
from pathlib import Path
from unittest.mock import patch

from outshine_ref.geometry.surface_patches import SurfacePatch, SurfacePatches
from stair_module_experiment import write_json
from stair_prepared_surface_scope import admitted_terrain, reference
from stair_resident_snapshot_cost import run


@contextlib.contextmanager
def geometric_work(work):
    construct, replace = SurfacePatch.__post_init__, SurfacePatches.replace

    def counted_construct(self):
        work["strict_patch_constructions"] += 1
        return construct(self)

    def counted_replace(self, *args, **kwargs):
        work["patch_replacements"] += 1
        return replace(self, *args, **kwargs)

    with (
        patch.object(SurfacePatch, "__post_init__", counted_construct),
        patch.object(SurfacePatches, "replace", counted_replace),
    ):
        yield


def measure(args):
    work_output = args.output.with_name(args.output.stem + "-prepared.json")
    if work_output.exists():
        raise ValueError("fresh separate prepared-load work report required")
    records = []
    work = {"strict_patch_constructions": 0, "patch_replacements": 0}
    scope = contextlib.nullcontext()
    if args.method != "strict":
        scope = admitted_terrain(
            args.prepared_reference, args.reference_sha256, args.method, records
        )
    with geometric_work(work), scope:
        result = run(
            argparse.Namespace(
                products=args.products,
                output=args.output,
                profile=args.profile,
                mode="snapshot",
            )
        )
    proof = reference(args.prepared_reference, args.reference_sha256)
    if result["state_sha256"] != proof["complete_state_sha256"]:
        raise ValueError("prepared load changed independently pinned whole state")
    if args.method != "strict" and (
        not any(row["restored"] for row in records) or any(work.values())
    ):
        raise ValueError("admitted hit did not bypass original geometric replay")
    report = {
        "commit": result["commit"],
        "method": args.method,
        "state_sha256": result["state_sha256"],
        "reference_sha256": args.reference_sha256,
        "work": work,
        "prepared_calls": records,
        "scope": "Same full roads/profile/terrain/support/headroom state. Prepared hits "
        "include lazy external-reference IO/hash, resident/terrain binding hashes, payload "
        "IO/hash/header/decode, structural mesh checks and generic polygons/STRtrees. "
        "Both modes count strict patch constructors/replacements; strict is the current "
        "normal library path, not the earlier general-GEOS baseline. Imports, preparation, "
        "independent admission and result hashing are excluded. Decoded-array counters "
        "include numeric metadata; they are not all copies or disk bytes. Expected digest "
        "is caller-owned and established independently, never inferred from the candidate. "
        "No native cache/device/world admission or zero-work claim.",
    }
    write_json(work_output, report)
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("products", "prepared-reference", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--reference-sha256", required=True)
    parser.add_argument(
        "--method", choices=("strict", "stored", "compressed"), required=True
    )
    parser.add_argument("--profile", type=Path)
    measure(parser.parse_args())
