"""Bound coupled roof plans/components to the original complete provider census."""

import argparse
import json
import resource
import time
from pathlib import Path

import numpy as np
import shapely
from evidence import revision
from terraced_admission_probe import records
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import plan
from terraced_crown_probe import roof_checks
from terraced_mass_admission import inspect
from terraced_mass_census import source_identity


def draw_checks(owner):
    mesh = owner.draw(True)
    if not all(np.isfinite(value).all() for value in mesh.values()):
        raise ValueError("Complete composed draw has nonfinite values")
    if not np.allclose(np.linalg.norm(mesh["normals"], axis=1), 1, atol=1e-6, rtol=0):
        raise ValueError("Complete composed draw has unresolved normals")
    if not np.array_equal(
        [mesh["vertices"][:, 2].min(), mesh["vertices"][:, 2].max()],
        [owner.plan.body.bottom_m, owner.plan.source_top_m],
    ):
        raise ValueError("Complete composed draw changed source heights")
    null_count = 0
    floors = owner.body.facade.near.floors
    for part in (floors.shell, floors.stairs, floors.landing):
        xyz = part.mesh["vertices"][part.mesh["faces"]]
        cross = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
        null_count += int(np.all(cross == 0, axis=1).sum())
    return {
        "triangles": len(mesh["faces"]),
        "closed_body_null_topology_facets": null_count,
        "finite_unit_normals": True,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--census", type=Path, required=True)
    parser.add_argument("--physical-roofs", action="store_true")
    parser.add_argument("--full-bodies", action="store_true")
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
    result, began = [], time.perf_counter()
    for row, programme in sources:
        identity = source_identity(row)
        record = identity | {
            "programme": programme,
            "full_body_checked": False,
            "full_draw_checked": False,
        }
        start, phase = time.perf_counter(), "plan"
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
            }
            owner = (
                RoofedProducts(current)
                if args.physical_roofs or args.full_bodies
                else None
            )
            if args.physical_roofs and current.crown:
                phase = "independent-roof-checks"
                record["roof_checks"] = roof_checks(owner)
                if owner.body.facade.near is not None:
                    raise ValueError("Roof-only audit expanded the full body")
                record["roof_components_passed"] = True
            if args.full_bodies:
                phase = "independent-body-checks"
                record["checks"] = inspect(owner.body)
                record["full_body_checked"] = True
                phase = "complete-draw-checks"
                record["draw_checks"] = draw_checks(owner)
                record |= {"passed": True, "full_draw_checked": True}
        except (ValueError, shapely.errors.GEOSException) as error:
            record |= {
                "planned": record.get("planned", False),
                "phase": phase,
                "reason": str(error),
                "fallback": "retain prior source programme",
            }
            if args.full_bodies:
                record["passed"] = False
        if identity != source_identity(row):
            raise ValueError("Roof audit changed the original source")
        record["seconds"] = time.perf_counter() - start
        result.append(record)
        print(
            json.dumps(
                {
                    k: record[k]
                    for k in ("id", "planned", "full_body_checked", "seconds")
                }
            ),
            flush=True,
        )
        args.output.write_text(
            json.dumps(
                proof
                | context
                | {
                    "records": result,
                    "original_census": original,
                    "completed": len(result) == len(sources),
                    "original_source_count": len(original),
                    "passed": sum(r.get("passed", False) for r in result),
                    "full_body_checks_requested": args.full_bodies,
                    "roof_checks_requested": args.physical_roofs,
                    "seconds": time.perf_counter() - began,
                    "worker_peak_rss_kib": resource.getrusage(
                        resource.RUSAGE_SELF
                    ).ru_maxrss,
                    "scope": "Original source binding and coupled2D roof/body plans; optional independent closed roofs and existing complete scoped body checks. Body scope covers shell/parts, sampled stairs, continuous ground/declared terrace routes; no continuous controller/highest roof access/whole-city/native admission.",
                },
                indent=2,
            )
            + "\n"
        )


if __name__ == "__main__":
    main()
