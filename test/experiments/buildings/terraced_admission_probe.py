"""Audit complete direct candidates or retained source counterexamples, only in /tmp."""

import argparse
import json
import resource
import time
from pathlib import Path

import shapely
from city_scene import inputs
from evidence import revision
from industrial_relief_probe import camera
from podium_probe import write
from stepped_source_probe import overview
from terraced_facade_probe import materials
from terraced_mass_admission import inspect
from terraced_mass_census import source_identity
from terraced_mass_plan import plan as mass_plan
from terraced_terrace_cost import arrays
from terraced_terrace_geometry import TerraceProducts
from terraced_terrace_plan import plan

DATA = Path(__file__).with_name("data")


def controls():
    data = json.loads((DATA / "terraced-admission-controls.json").read_text())
    return [
        dict(r, polygon=shapely.from_wkb(r["footprint_wkb"])) for r in data["cases"]
    ]


def records(args):
    if args.census is None:
        return [(r, r["programme"]) for r in controls()], [], {}
    reference = json.loads(args.census.read_text())
    rows, receipts, window = inputs(
        "Paris", json.loads((DATA / "roof-crown-city-window.json").read_text())
    )
    buildings = {r["id"]: r for r in rows if r["layer"] == "building"}
    old = reference["records"]
    if len(old) != 988 or {r["id"] for r in old} != set(buildings):
        raise ValueError("Physical audit requires the full unchanged988-source census")
    protected = set(
        json.loads((DATA / "terraced-city-baseline.json").read_text())[
            "existing_function_ids"
        ]
    )
    for r in old:
        if any(r[k] != v for k, v in source_identity(buildings[r["id"]]).items()):
            raise ValueError("Census source geometry/properties changed")
        if (r["id"] in protected) != (
            r["status"] == "retained-existing-functional-owner"
        ):
            raise ValueError("Existing functional ownership changed")
    candidates = [r for r in old if r["status"] == "direct-candidate"]
    if len(candidates) != 71:
        raise ValueError("Original71 proposed programmes changed")
    return (
        [(buildings[r["id"]], r["programme"]) for r in candidates],
        old,
        {"receipts": receipts, "window": window},
    )


def export(owner, output, index, proof, checks):
    mesh = owner.draw(True)
    view = overview(mesh["vertices"], [1, -1.25, 0.85])
    mass = owner.plan.prepared.mass
    write(
        output,
        f"control-{index}",
        mesh,
        {
            "source_id": mass.identifier,
            "source_footprint_wkb": mass.source_wkb.hex(),
            "source": {"material_definitions": materials()},
            "facade_materials": {},
            "ground_span_m": 10000,
            "camera": camera(view),
            "physical_checks": checks,
            "scope": "Actual corrected counterexample, full near exterior with shared source/procedural materials, flat ground. No whole-city/native/frame/night admission.",
        },
        proof,
    )


def run(args):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the experiment before physical evidence")
    sources, original, context = records(args)
    audit, began = [], time.perf_counter()
    for index, (row, programme) in enumerate(sources):
        start, phase = time.perf_counter(), "plan"
        identity = source_identity(row)
        try:
            owner = TerraceProducts(plan(mass_plan(row, programme)))
            phase = "physical"
            owner.facade.physical()
            phase = "independent-physical-checks"
            checks = inspect(owner)
            result = {
                "passed": True,
                "checks": checks,
                "resident_full_payload": arrays(owner, True),
            }
            if args.models is not None:
                phase = "model-export"
                export(owner, args.models, index, proof, checks)
        except (ValueError, shapely.errors.GEOSException) as error:
            result = {
                "passed": False,
                "phase": phase,
                "reason": str(error),
                "fallback": "retain previous source route",
            }
        if source_identity(row) != identity:
            raise ValueError("Physical work changed its original source")
        audit.append(
            identity
            | {"programme": programme, "seconds": time.perf_counter() - start}
            | result
        )
        print(
            json.dumps(
                {
                    "source": index,
                    "id": row["id"],
                    "passed": result["passed"],
                    "seconds": audit[-1]["seconds"],
                }
            ),
            flush=True,
        )
        args.output.write_text(
            json.dumps(
                proof
                | context
                | {
                    "records": audit,
                    "original_census": original,
                    "completed": len(audit) == len(sources),
                    "passed": sum(r["passed"] for r in audit),
                    "seconds": time.perf_counter() - began,
                    "worker_peak_rss_kib": resource.getrusage(
                        resource.RUSAGE_SELF
                    ).ru_maxrss,
                },
                indent=2,
            )
            + "\n"
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--census", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--models", type=Path)
    args = parser.parse_args()
    for path in (args.output.parent, args.models):
        if path is not None and (
            path.resolve() == Path("/tmp")
            or not path.resolve().is_relative_to(Path("/tmp"))
        ):
            parser.error("Outputs require an explicit subdirectory of /tmp")
        if path is not None:
            path.mkdir(parents=True, exist_ok=True)
    run(args)


if __name__ == "__main__":
    main()
