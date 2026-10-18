"""Same source/camera compact-versus-physical facade inputs; all outputs in system temp."""

import argparse
import hashlib
import json
from dataclasses import replace
from pathlib import Path

import numpy as np
import shapely
from demand import View
from evidence import revision
from opening_details import slots
from stepped_facade_details import demand, present
from stepped_profile_probe import save, sources
from stepped_source import plan_and_near
from stepped_source_probe import overview


def views(row, plan, index):
    bounds = row["polygon"].bounds
    points = np.array(
        [
            [x, y, z]
            for x in (bounds[0], bounds[2])
            for y in (bounds[1], bounds[3])
            for z in (plan["base_z_m"], plan["source_top_m"])
        ]
    )
    whole = overview(points, [1.0, -1.25, 0.8])
    result = [("Whole", whole)]
    if index == 0:
        result.append(("Opposite", overview(points, [-1.0, 1.25, 0.8])))
        candidates = [
            o
            for o in slots(plan)
            if o["kind"] == "window"
            and row["polygon"].exterior.distance(shapely.Point(o["centre"])) < 1e-7
        ]
        opening = max(
            candidates,
            key=lambda o: np.dot(
                whole.eye[:2] - o["centre"], [o["tangent"][1], -o["tangent"][0]]
            ),
        )
        centre, tangent = np.asarray(opening["centre"]), np.asarray(opening["tangent"])
        normal = np.array([tangent[1], -tangent[0]])
        target = np.r_[centre, opening["bottom_z_m"] + opening["height_m"] / 2]
        eye = target + np.r_[normal * 5.0 + tangent * 0.5, 0.3]
        result.append(("Near", View(eye, target, width=1920, height=1080, fov_deg=48)))
        result.append(
            ("Far", replace(whole, eye=whole.target + (whole.eye - whole.target) * 30))
        )
    return result


def run(output):
    provenance = revision()
    if provenance["experiment_tree_dirty"]:
        raise ValueError("Freeze the source before physical facade comparisons")
    output.mkdir(parents=True, exist_ok=True)
    results = []
    for index, profile in ((0, "corner"), (2, "diagonal")):
        row = sources()[index]
        plan, payload = plan_and_near(row, profile)
        for suffix, view in views(row, plan, index):
            before, name = f"{index}Compact{suffix}", f"{index}Physical{suffix}"
            compact, _ = save(row, plan, view, output, before, provenance)
            resolved, choice = demand(plan, view)
            mesh = present(payload[0], 1, 32768) if resolved else compact
            path = (
                output / (name + "-mesh.npz")
                if resolved
                else output / (before + "-mesh.npz")
            )
            if resolved:
                np.savez_compressed(path, **mesh)
            record = json.loads((output / (before + "-geometry.json")).read_text())
            record["compact_reference_geometry"] = record.pop("geometry")
            record.update(
                city=name,
                mesh=str(path.resolve()),
                mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                triangles=len(mesh["faces"]),
                facade_presentation=choice
                | {"mode": "near" if resolved else "compact"},
                actual_near_access=payload[1],
                actual_terrace_paths=payload[2],
                scope="Exact retained physical near assembly with presentation-only metric glass binding, or unchanged compact source exterior; no native/controller/image-LOD/device/night admission",
            )
            (output / (name + "-geometry.json")).write_text(
                json.dumps(record, indent=2) + "\n"
            )
            results.append(
                {
                    "source_id": row["id"],
                    "before": before,
                    "after": name,
                    "resolved": resolved,
                    "triangles_before": len(compact["faces"]),
                    "triangles_after": len(mesh["faces"]),
                    "room_ids": plan["storey_room_ids"],
                }
            )
    (output / "comparison.json").write_text(
        json.dumps(provenance | {"cases": results}, indent=2) + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
