"""Complete pinned source census of direct terraced candidates, retaining prior owners."""

import argparse
import hashlib
import json
import resource
import time
from collections import Counter
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from city_scene import inputs
from evidence import revision
from outshine_ref.buildings.structures import classify_structure
from provider_contract import attributes, seed_for
from terraced_mass_plan import plan as mass_plan
from terraced_terrace_cost import arrays
from terraced_terrace_geometry import TerraceProducts
from terraced_terrace_plan import plan
from tile_podiums import eligible
from tile_room_details import numeric_properties


def digest(identifiers):
    return hashlib.sha256(
        json.dumps(sorted(identifiers), separators=(",", ":")).encode()
    ).hexdigest()


def candidates(row):
    first = (
        "cascade" if seed_for(row["id"] + "/terraced-kind") % 2 == 0 else "paired-wings"
    )
    failures = []
    for kind in (first, "paired-wings" if first == "cascade" else "cascade"):
        phase, began = "mass", time.perf_counter()
        try:
            mass = mass_plan(row, kind)
            phase = "terrace-plan"
            prepared = plan(mass)
            phase = "direct-product"
            owner = TerraceProducts(prepared)
            mesh = owner.draw(False)
            if owner.facade.near is not None:
                raise AssertionError("Direct census expanded a physical near product")
            if not all(np.isfinite(a).all() for a in mesh.values()):
                raise ValueError("Direct source product has nonfinite arrays")
            if not np.array_equal(
                [mesh["vertices"][:, 2].min(), mesh["vertices"][:, 2].max()],
                [mass.bottom_m, mass.top_m],
            ):
                raise ValueError("Direct source product changed source height")
            if mass.source_wkb != row["polygon"].wkb:
                raise ValueError(
                    "Direct source product changed original footprint identity"
                )
            return {
                "status": "direct-candidate",
                "programme": kind,
                "storeys": len(mass.storeys),
                "windows": len(prepared.prepared.facade.windows),
                "terraces": len(prepared.terraces),
                "portals": len(prepared.portals),
                "unserved_terraces": sum(
                    t.reason is not None for t in prepared.terraces
                ),
                "draw_triangles": len(mesh["faces"]),
                "resident_direct_payload": arrays(owner, False),
                "seconds": time.perf_counter() - began,
                "previous_attempts": failures,
            }
        except (ValueError, shapely.errors.GEOSException) as error:
            failures.append(
                {
                    "programme": kind,
                    "phase": phase,
                    "reason": str(error),
                    "seconds": time.perf_counter() - began,
                }
            )
    return {"status": "retained-no-direct-candidate", "attempts": failures}


def census(rows, baseline, *, progress):
    buildings = [r for r in rows if r["layer"] == "building"]
    identifiers = [r["id"] for r in buildings]
    if (
        len(identifiers) != 988
        or len(set(identifiers)) != len(identifiers)
        or digest(identifiers) != baseline["source_ids_sha256"]
    ):
        raise ValueError("Full original source membership changed")
    requested = {
        r["id"]
        for r in buildings
        if eligible(r) and seed_for(r["id"] + "/podium") % 4 == 0
    }
    if len(requested) != 80 or digest(requested) != baseline["requested_ids_sha256"]:
        raise ValueError("Original80-request comparison cohort changed")
    protected = set(baseline["existing_function_ids"])
    if not set(baseline["existing_podium_ids"]).issubset(
        protected
    ) or not protected.issubset(identifiers):
        raise ValueError("Existing functional ownership reference is incomplete")
    records = []
    for row in buildings:
        source = {
            "id": row["id"],
            "source_footprint_sha256": hashlib.sha256(row["polygon"].wkb).hexdigest(),
            "source_properties_sha256": hashlib.sha256(
                json.dumps(dict(row["properties"]), sort_keys=True).encode()
            ).hexdigest(),
        }
        if row["id"] in protected:
            result = {"status": "retained-existing-functional-owner"}
        elif row["id"] not in requested:
            result = {"status": "retained-outside-existing-request-cohort"}
        else:
            decision = classify_structure(
                row["polygon"],
                row["properties"],
                coordinate_step_m=row["coordinate_step_m"],
                clipped=row["clipped"],
            )
            p = attributes(numeric_properties(row))
            result = (
                candidates(row)
                if decision.kind == "ordinary" and not p["hide3d"]
                else {"status": "retained-special-source", "kind": decision.kind}
            )
        records.append(source | result)
        if progress and row["id"] in requested:
            print(
                json.dumps(
                    {
                        "accounted": len(records),
                        "id": row["id"],
                        "status": result["status"],
                    }
                ),
                flush=True,
            )
    if {
        r["id"] for r in records if r["status"] == "retained-existing-functional-owner"
    } != protected:
        raise ValueError(
            "Direct candidate census replaced an existing functional owner"
        )
    accepted = [r for r in records if r["status"] == "direct-candidate"]
    return {
        "records": records,
        "summary": {
            "source_buildings": len(buildings),
            "existing_requests": len(requested),
            "protected_functional_owners": len(protected),
            "protected_podiums": len(baseline["existing_podium_ids"]),
            "status_counts": dict(Counter(r["status"] for r in records)),
            "programmes": dict(Counter(r["programme"] for r in accepted)),
            "candidate_draw_triangles": sum(r["draw_triangles"] for r in accepted),
            "candidate_windows": sum(r["windows"] for r in accepted),
            "candidate_portals": sum(r["portals"] for r in accepted),
            "peak_individual_direct_numpy_bytes": max(
                (r["resident_direct_payload"]["output_numpy_bytes"] for r in accepted),
                default=0,
            ),
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("--output requires a file under /tmp")
    proof = revision()
    if proof["experiment_tree_dirty"]:
        parser.error("Freeze the census implementation first")
    root = Path(__file__).with_name("data")
    baseline = json.loads((root / "terraced-city-baseline.json").read_text())
    rows, receipts, window = inputs(
        "Paris", json.loads((root / "roof-crown-city-window.json").read_text())
    )
    actual = sorted((r["url"], r["sha256"]) for r in receipts["receipts"])
    if actual != sorted((r["url"], r["sha256"]) for r in baseline["receipts"]):
        raise ValueError("Actual source receipts changed")
    began = time.perf_counter()
    with patch(
        "terraced_facade_geometry.build",
        side_effect=AssertionError("Unrequested physical floor expansion"),
    ):
        result = census(rows, baseline, progress=True)
    result |= proof | {
        "sources": receipts,
        "window": window,
        "census_seconds": time.perf_counter() - began,
        "worker_peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "All988 original source IDs accounted; old80-request cohort and12 functional owners/8 podiums pinned. Direct candidates only; no changed whole-city output, no near physical/access/closed-volume/light/shadow/cache/native/device acceptance for new sources. Unselected, failed and previously functional sources retain their previous ownership; their geometry is not regenerated by this census. Source acquisition is excluded from census time; no automatic source refresh.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result["summary"]))


if __name__ == "__main__":
    main()
