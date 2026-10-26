"""Equal-solid primitive alternatives and complete optional service generation costs."""

import argparse
import json
import os
import resource
import sys
from functools import partial
from pathlib import Path

import numpy as np
import shapely
from cooling_service_mesh import components, world_points
from cooling_service_plan import plan
from cooling_tower_details import refine
from cooling_tower_probe import sources
from evidence import revision
from industrial_rooflight_probe import storage_bytes
from outshine_ref.buildings.modelling import Shape
from outshine_ref.buildings.rooflight_panels import triangle_surface
from recipe_hall_operators import sealed
from rooflight_lamina_probe import measured
from structural_shell_geometry import native


def general_components(access):
    result = {}
    for part in access.parts:
        x, y, z = np.array(part.extent) / 2
        raw = (
            Shape.from_polygon(shapely.box(-x, -y, x, y), elevation=-z)
            .extrude(2 * z)
            .emit()
        )
        points = raw["vertices"][:, [0, 2, 1]] * [1, -1, 1] + part.centre
        result[part.identifier] = sealed(
            triangle_surface(
                {"vertices": world_points(access, points), "faces": raw["faces"]},
                access.material,
            )
        )
    return result


def check_alternative(a, b):
    if a.keys() != b.keys():
        raise ValueError("component alternatives changed service identities")
    maximum = 0.0
    for key in a:
        first, second = native(a[key]), native(b[key])
        error = max((first - second).volume(), (second - first).volume())
        if error > 1e-8 or len(a[key]["faces"]) != len(b[key]["faces"]):
            raise ValueError("general primitive changed actual service solid/capacity")
        maximum = max(maximum, error)
        for mesh in (a[key], b[key]):
            if not np.all(mesh["materials"] == a[key]["materials"][0]):
                raise ValueError("service primitive changed material binding")
            xyz, uv = mesh["vertices"][mesh["faces"]], mesh["uv"][mesh["faces"]]
            for i, j in ((0, 1), (1, 2), (2, 0)):
                np.testing.assert_allclose(
                    np.linalg.norm(xyz[:, i] - xyz[:, j], axis=1),
                    np.linalg.norm(uv[:, i] - uv[:, j], axis=1),
                    rtol=0,
                    atol=1e-10,
                )
    return maximum


def costs(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("freeze service construction before cost evidence")
    _rows, _receipt, _view, cases = sources()
    entries = []
    for row, tower in cases:
        access, planning = measured(partial(plan, tower))
        a, b = components(access), general_components(access)
        error = check_alternative(a, b)
        alternatives = []
        for label, call in (
            ("standard-boxes", partial(components, access)),
            ("general-shape-boxes", partial(general_components, access)),
        ):
            result, timing = measured(call)
            alternatives.append(
                timing
                | {
                    "method": label,
                    "components": len(result),
                    "triangles": sum(len(m["faces"]) for m in result.values()),
                    "final_arrays": sum(len(m) for m in result.values()),
                    "unique_array_bytes": storage_bytes(list(result.values())),
                    "logical_array_bytes": sum(
                        v.nbytes for m in result.values() for v in m.values()
                    ),
                }
            )
        complete = []
        for enabled in (False, True):
            result, timing = measured(
                partial(
                    refine,
                    row,
                    tower.source,
                    {"source_kind": "cooling_tower"},
                    0.2,
                    service_access=enabled,
                )
            )
            complete.append(
                timing
                | {
                    "service_access": enabled,
                    "triangles": len(result[0]["faces"]),
                    "unique_array_bytes": storage_bytes([result[0]]),
                    "final_arrays": len(result[0]),
                }
            )
        entries.append(
            {
                "source_id": row["id"],
                "planning": planning,
                "alternatives": alternatives,
                "maximum_solid_difference_m3": error,
                "complete_refine": complete,
            }
        )
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    report = proof | {
        "load_average": os.getloadavg(),
        "sources": entries,
        "process_peak_rss_bytes": int(rss if sys.platform == "darwin" else rss * 1024),
        "physical_allocation_count_and_total_copied_bytes": None,
        "scope": "Python first/subsequent generation with existing measured helper and separate traced-peak calls. Primitive alternatives preserve actual solids, face capacity, material and metric UV. Complete refine includes previous tower generation, new service and final draw packing; source decode, booleans, rendering and native/DB/device costs excluded. Tracemalloc misses parts of the native heap; final arrays are not all allocations or memory traffic. No warm asset-cache or frame speedup claim.",
    }
    output.mkdir(parents=True, exist_ok=False)
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "sources": len(entries),
                "process_peak_rss_bytes": report["process_peak_rss_bytes"],
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    costs(parser.parse_args().output)
