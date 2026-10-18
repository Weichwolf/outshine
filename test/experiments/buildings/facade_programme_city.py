"""Replace owned facade trim in the whole original source window, checking actual bodies."""

import argparse
import hashlib
import json
import resource
import time
from dataclasses import replace
from pathlib import Path

import numpy as np
from bay_city_checks import inspect as inspect_layers
from bay_facade_plan import FIELD_MATERIAL
from bay_facade_probe import palette as field_palette
from city_scene import inputs
from evidence import revision
from facade_programme_plan import PROGRAMMES, plan_programme
from facade_programme_products import palette, programme_products
from showcase_tiles import view_for
from terraced_city import emit
from terraced_city_parts import PreviousParts
from terraced_city_reference import admitted, unchanged_modelling
from terraced_crown_plan import plan
from terraced_crown_probe import roof_checks
from terraced_mass_admission import inspect
from terraced_mass_census import source_identity
from terraced_terrace_plan import plan as terraces
from tile_dormers import DormerFacades
from tile_podiums import PodiumDetails
from tile_terraced import bound
from tile_terraced_crowns import CrownedDetails

DATA = Path(__file__).with_name("data")


class ProgrammeDetails(CrownedDetails):
    def __init__(self, previous, records, programme):
        super().__init__(previous, records)
        self.programme = programme

    def __call__(self, row, parameters, material):
        identifier = row["id"]
        reference = self.records.get(identifier)
        if reference is None:
            return self.previous(row, parameters, material)
        if row is not self.owner.source_rows[identifier] or any(
            reference[k] != v for k, v in source_identity(row).items()
        ):
            raise ValueError("Programme city changed an original source owner")
        if identifier in self.assets:
            raise ValueError("Whole city repeated a source programme")
        began = time.perf_counter()
        crown = plan(row, reference["programme"])
        # Plan from the same camera-independent logical terrace as the body factory.
        terrace = terraces(crown.body)
        planned = plan_programme(
            terrace.prepared, self.programme, portals=terrace.portals
        )
        asset = programme_products(crown, planned)
        self.assets[identifier] = asset
        decision = asset.source.select(self.owner.view)
        planned_seconds = time.perf_counter() - began
        checked_at = time.perf_counter()
        physical = inspect(asset.body)
        layers = inspect_layers(planned, terrace)
        roof = roof_checks(asset.source) if crown.crown is not None else None
        diagnostic_seconds = time.perf_counter() - checked_at
        body_near = decision.get("body_mode", decision["mode"]) == "near"
        roof_near = decision.get("roof_mode", decision["mode"]) == "near"
        mesh = bound(asset.draw(body_near=body_near, roof_near=roof_near), material)
        if not np.array_equal(
            [mesh["vertices"][:, 2].min(), mesh["vertices"][:, 2].max()],
            [crown.body.bottom_m, crown.source_top_m],
        ) or not all(np.isfinite(v).all() for v in mesh.values()):
            raise ValueError("Programme city changed source bounds or finite geometry")
        self.completed.append(
            source_identity(row)
            | {
                "programme": self.programme,
                "body_programme": crown.body.kind,
                "product": "outer" if body_near else "front",
                "normal_demand": decision,
                "cells": len(planned.cells),
                "rejected": planned.rejected,
                "passed": True,
                "physical_checks": physical,
                "layer_checks": layers,
                "roof_checks": roof,
                "triangles": len(mesh["faces"]),
                "planning_seconds": planned_seconds,
                "diagnostic_seconds": diagnostic_seconds,
                "seconds": time.perf_counter() - began,
            }
        )
        print(
            json.dumps(
                {
                    "programme": self.programme,
                    "checked_source": len(self.completed),
                    "id": identifier,
                    "closed_layers": layers["closed_layers"],
                }
            ),
            flush=True,
        )
        return mesh, {
            "form": "source-height-contained body/roof and complete facade programme",
            "programme": self.programme,
            "functional_status": physical["scope"],
        }


def run(args):
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("Fresh explicit private city output required")
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the full facade city first")
    reference = json.loads(args.audit.read_text())
    unchanged_modelling(reference)
    baseline = json.loads((DATA / "terraced-city-baseline.json").read_text())
    rows, receipts, window = inputs(
        "Paris", json.loads((DATA / "roof-crown-city-window.json").read_text())
    )
    records = admitted(rows, reference, baseline)
    if len(reference["original_census"]) != 988 or len(records) != 71:
        raise ValueError("Complete988-source/71-body input required")
    if sorted((r["url"], r["sha256"]) for r in receipts["receipts"]) != sorted(
        (r["url"], r["sha256"]) for r in baseline["receipts"]
    ):
        raise ValueError("Original whole-window acquisition changed")
    view = replace(view_for(rows), width=1920, height=1080)
    owner = PodiumDetails(rows, view)
    owner.facades = DormerFacades(owner.rows, view)
    previous = PreviousParts(owner)
    args.output.mkdir(parents=True)
    context = {
        "physical_reference": str(args.audit.resolve()),
        "physical_reference_sha256": hashlib.sha256(
            args.audit.read_bytes()
        ).hexdigest(),
    }
    before = emit(
        args.output,
        "ProgrammeBefore",
        previous,
        previous,
        owner.rows,
        window,
        view,
        receipts,
        proof,
        context,
    )
    original = json.loads(args.before.read_text())
    if (
        hashlib.sha256(Path(original["mesh"]).read_bytes()).hexdigest()
        != original["mesh_sha256"]
    ):
        raise ValueError("Original previous city geometry bytes changed")
    if (
        before["mesh_array_sha256"] != original["mesh_array_sha256"]
        or before["sources"] != original["sources"]
        or before["window"] != original["window"]
        or before["camera"] != original["camera"]
    ):
        raise ValueError("Fresh complete previous geometry/source/camera changed")
    protected = {r["id"] for r in owner.completed}
    if (
        not set(baseline["existing_function_ids"]) <= protected
        or set(records) & protected
    ):
        raise ValueError("Whole city changed protected functional ownership")
    builder = ProgrammeDetails(previous, records, args.programme)
    definitions = {
        str(FIELD_MATERIAL): field_palette()[str(FIELD_MATERIAL)]
    } | palette()
    record = emit(
        args.output,
        "ProgrammeAfter",
        builder,
        previous,
        owner.rows,
        window,
        view,
        receipts,
        proof,
        context
        | {
            "new_sources": builder.completed,
            "protected_existing_sources": sorted(protected),
            "programme": args.programme,
            "previous_mesh_array_sha256": before["mesh_array_sha256"],
        },
        material_definitions=definitions,
        scope="Complete original988-source flat window,71 coupled source bodies with fresh independent whole collision/access/layer/roof checks. Coherent facade programmes replace owned trim; previous functional owners remain. Position-only body/roof draw demand unchanged; physical diagnostics are separate work. No controller/cache/native/frame/night/whole-world admission.",
    )
    if len(builder.completed) != 71 or {r["id"] for r in builder.completed} != set(
        records
    ):
        raise ValueError("Whole programme city lost a source owner")
    result = {
        "commit": proof["commit"],
        "programme": args.programme,
        "sources": record["source"]["generated"],
        "checked_new_owners": len(builder.completed),
        "protected_owners": len(baseline["existing_function_ids"]),
        "complete": True,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
    }
    (args.output / "programme-city.json").write_text(
        json.dumps(result, indent=2) + "\n"
    )
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("audit", "before", "output"):
        parser.add_argument("--" + key, type=Path, required=True)
    parser.add_argument("--programme", choices=PROGRAMMES, required=True)
    run(parser.parse_args())
