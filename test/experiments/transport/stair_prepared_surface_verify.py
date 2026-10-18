"""Cold-compare candidate arrays to original strict history before publishing a pin."""

import argparse
import json
import resource
from pathlib import Path

from camera_height_inputs import digest
from outshine_ref.contracts import LocalFrame
from outshine_ref.geometry.surface_patches.prepared import FORMAT, restore_surface
from stair_composition_scope import composition
from stair_coverage_boundary_scope import validation
from stair_local_replay import load_chain
from stair_module_experiment import write_json
from stair_module_inputs import read_arrays
from stair_patch_experiment import compare
from stair_patch_verify import compare_queries
from stair_prepared_surface_scope import REFERENCE
from stair_resident_snapshot_cost import state_hash
from stair_resident_snapshot_verify import equal_patches
from stair_terrain_experiment import frozen


def verify(args):
    commit = frozen()
    target = args.output or args.products / "reference.json"
    if target.exists() or not target.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("fresh explicit private independent reference required")
    report = json.loads((args.products / "products.json").read_text())
    resident = Path(report["resident"])
    for name, expected in (
        ("resident.json", report["resident_sha256"]),
        ("comparison.json", report["resident_report_sha256"]),
        ("verification.json", report["resident_receipt_sha256"]),
    ):
        if digest(resident / name) != expected:
            raise ValueError("original independently verified resident binding changed")
    if report["format"] != FORMAT:
        raise ValueError("candidate codec version changed")
    if type(report["variants"]) is not dict or set(report["variants"]) != {
        "stored",
        "compressed",
    }:
        raise ValueError("both complete candidate representations required")
    with validation("general", []), composition("general", []):
        before = load_chain(resident)
    if (
        before[4]["frame"] != report["frame"]
        or before[4]["terrain_sha256"] != report["terrain_sha256"]
    ):
        raise ValueError("original full terrain/frame binding changed")
    results = {}
    for name, row in report["variants"].items():
        path = (args.products / row["file"]).resolve()
        if (
            not path.is_relative_to(args.products.resolve())
            or digest(path) != row["sha256"]
        ):
            raise ValueError("candidate numeric bytes changed")
        # A candidate is inspected only for comparison, never admitted to a caller here.
        after = restore_surface(
            path.read_bytes(),
            expected_sha256=row["sha256"],
            frame=LocalFrame(report["frame"]),
        )
        equal_patches(after, before[0])
        if after.max_patches != before[0].max_patches or any(
            (a.kind, a.max_faces) != (b.kind, b.max_faces)
            for a, b in zip(after.patches, before[0].patches, strict=True)
        ):
            raise ValueError(
                "candidate kind/capacity differs from original strict surface"
            )
        whole = after.query(after.patches[0].bounds_xy_m, max_faces=65536)
        equality = compare(
            after,
            read_arrays(resident / "world-layout.npz"),
            read_arrays(resident / "world-field.npz"),
        )
        if whole.mesh is None:
            raise ValueError("candidate full query failed after exact comparison")
        queries = compare_queries(after, whole.mesh)
        if state_hash((after, *before[1:])) != state_hash(before):
            raise ValueError("candidate changed complete transport state")
        results[name] = {"equality": equality, "queries": queries}
    reference = {
        "reference_format": REFERENCE,
        "codec_format": FORMAT,
        "reader_commit": commit,
        "products": str(args.products.resolve()),
        "products_sha256": digest(args.products / "products.json"),
        "resident": report["resident"],
        "resident_sha256": report["resident_sha256"],
        "terrain": report["terrain"],
        "terrain_sha256": report["terrain_sha256"],
        "frame": report["frame"],
        "variants": report["variants"],
        "complete_state_sha256": state_hash(before),
        "checks": results,
        "scope": "Exact candidate/source arrays, IDs, masks, attributes and complete-state "
        "equality against independent strict construction/general composition. The caller "
        "must pin this reference's digest externally; its mere existence or a self-computed "
        "payload hash does not authorize fast restoration.",
    }
    write_json(target, reference)
    print(
        json.dumps(
            {
                "reference": str(target),
                "external_reference_sha256": digest(target),
                "checks": results,
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    verify(parser.parse_args())
