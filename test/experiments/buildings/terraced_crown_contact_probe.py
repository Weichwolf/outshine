"""Reproduce the actual null-topology source as a complete paired flat model."""

import argparse
import json
from pathlib import Path

import numpy as np
from evidence import revision
from industrial_relief_probe import camera
from podium_probe import write
from provider_contract import srgb
from stepped_source_probe import overview
from terraced_admission_probe import controls
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import plan
from terraced_crown_probe import roof_checks
from terraced_facade_probe import materials
from terraced_mass_admission import inspect
from terraced_mass_plan import plan as mass_plan
from terraced_terrace_geometry import TerraceProducts
from terraced_terrace_plan import plan as terrace_plan
from tile_terraced import indices
from tile_terraced_crowns import crown_definitions


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the contact producer before model evidence")
    row = controls(Path(__file__).with_name("data") / "terraced-crown-contact.json")[0]
    owner = RoofedProducts(plan(row, row["programme"]))
    checks = {"body": inspect(owner.body), "roof": roof_checks(owner)}
    before = TerraceProducts(terrace_plan(mass_plan(row, row["programme"]))).draw(True)
    before = dict(before, faces=indices(before))
    after = owner.draw(True)
    if not all(np.isfinite(value).all() for value in after.values()):
        raise ValueError("Contact model has a nonfinite draw field")
    view = overview(np.r_[before["vertices"], after["vertices"]], [1, -1.25, 0.85])
    palette = materials() | {"60003": crown_definitions()["60003"]}
    palette["12"].update(
        {
            "color." + axis: float(value)
            for axis, value in zip("rgb", srgb(row["properties"]), strict=True)
        }
    )
    record = {
        "source_id": row["id"],
        "source_properties": row["properties"],
        "source_footprint_wkb": row["polygon"].wkb_hex,
        "source": {"material_definitions": palette},
        "facade_materials": {},
        "camera": camera(view),
        "ground_span_m": 10000,
        "physical_checks": checks,
        "physical_check_product": "Coupled After body and roof; Before is the draw-only original body reference.",
        "scope": "Actual complete null-topology counterexample, original26m/9storeys and supplied beige colour. Same actual camera/light/materials for bare/coupled roof. Closed topology/positions retained; exact null draw facets omitted. No controller/native/device/radiance admission.",
    }
    output.mkdir(parents=True, exist_ok=True)
    for label, mesh in (("Before", before), ("After", after)):
        write(output, "CrownContact" + label, mesh, record, proof)
        print(label, len(mesh["faces"]), flush=True)
    (output / "checks.json").write_text(json.dumps(proof | checks, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == Path("/tmp") or not output.is_relative_to(Path("/tmp")):
        parser.error("--output requires an explicit subdirectory of /tmp")
    models(output)


if __name__ == "__main__":
    main()
