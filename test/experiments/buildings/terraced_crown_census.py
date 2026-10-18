"""Bound coupled roof plans/components to the original complete provider census."""

import argparse
import json
from pathlib import Path

import shapely
from evidence import revision
from terraced_admission_probe import records
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import plan
from terraced_crown_probe import roof_checks
from terraced_mass_census import source_identity


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--census", type=Path, required=True)
    parser.add_argument("--physical-roofs", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("--output requires a file under /tmp")
    proof = revision()
    if proof["experiment_tree_dirty"]:
        parser.error("Freeze the roof cohort first")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sources, original, context = records(args)
    previous = {r["id"]: r for r in original}
    result = []
    for row, programme in sources:
        identity = source_identity(row)
        record = identity | {"programme": programme}
        try:
            current = plan(row, programme)
            if len(current.body.storeys) != previous[row["id"]]["storeys"]:
                raise ValueError("Coupled roof changed the original storey count")
            record |= {
                "planned": True,
                "roof_kind": current.crown.kind
                if current.crown
                else "retained bare roof",
                "source_top_m": current.source_top_m,
                "body_top_m": current.body.top_m,
                "minimum_storey_m": min(
                    s.top_m - s.bottom_m for s in current.body.storeys
                ),
                "attempts": current.attempts,
                "full_body_checked": False,
            }
            if args.physical_roofs and current.crown:
                owner = RoofedProducts(current)
                record["roof_checks"] = roof_checks(owner)
                if owner.body.facade.near is not None:
                    raise ValueError("Roof-only audit expanded the full body")
                record["roof_components_passed"] = True
        except (ValueError, shapely.errors.GEOSException) as error:
            record |= {
                "planned": False,
                "reason": str(error),
                "fallback": "retain prior source programme",
            }
        if identity != source_identity(row):
            raise ValueError("Roof audit changed the original source")
        result.append(record)
        print(json.dumps({k: record[k] for k in ("id", "planned")}), flush=True)
        args.output.write_text(
            json.dumps(
                proof
                | context
                | {
                    "records": result,
                    "completed": len(result) == len(sources),
                    "original_source_count": len(original),
                    "scope": "Original source binding and coupled2D roof/body plans; optional independent closed roof components. No changed-body stair/arrival/terrace or whole-city/native/controller admission.",
                },
                indent=2,
            )
            + "\n"
        )


if __name__ == "__main__":
    main()
