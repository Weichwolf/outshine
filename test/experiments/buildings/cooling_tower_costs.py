"""Same complete intake components using standard boxes or general Shape extrusion."""

import argparse
import json
import os
from pathlib import Path

import numpy as np
import shapely
from cooling_tower_mesh import beam, components, placed_support
from cooling_tower_probe import sources
from evidence import revision
from industrial_rooflight_probe import storage_bytes
from outshine_ref.buildings.modelling import Shape
from rooflight_lamina_probe import measured
from structural_shell_geometry import native


def general_beam(support, material):
    half = support.width_m * 0.5
    height = support.end[2] - support.start[2]
    raw = (
        Shape.from_polygon(shapely.box(-half, -half, half, half), elevation=-height / 2)
        .extrude(height)
        .emit()
    )
    enu = dict(raw, vertices=raw["vertices"][:, [0, 2, 1]] * [1, -1, 1])
    return placed_support(enu, support, material)


def compare_support(support):
    a, b = beam(support, 1), general_beam(support, 1)
    aa, bb = native(a), native(b)
    if (aa - bb).volume() > 1e-8 or (bb - aa).volume() > 1e-8:
        raise ValueError("General extrusion changed the actual support solid")
    for mesh in (a, b):
        if len(mesh["faces"]) != 12 or not np.all(mesh["materials"] == 1):
            raise ValueError("Support comparison changed output capacity/material")
        xyz, uv = mesh["vertices"][mesh["faces"]], mesh["uv"][mesh["faces"]]
        for i, j in ((0, 1), (1, 2), (2, 0)):
            np.testing.assert_allclose(
                np.linalg.norm(xyz[:, i] - xyz[:, j], axis=1),
                np.linalg.norm(uv[:, i] - uv[:, j], axis=1),
                atol=1e-10,
            )


def costs(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze cooling construction before costs")
    _rows, _receipt, _view, cases = sources()
    rows, load = [], os.getloadavg()
    for _row, programme in cases:
        for support in programme.supports:
            compare_support(support)
        entries = []
        for method, backend in (
            ("trimesh-box-composition", beam),
            ("general-shape-composition", general_beam),
        ):
            result, timing = measured(
                lambda b=backend, p=programme: components(p, beam_builder=b)
            )
            entries.append(
                timing
                | {
                    "method": method,
                    "components": len(result),
                    "triangles": sum(len(m["faces"]) for m in result.values()),
                    "new_final_geometry_arrays": sum(len(m) for m in result.values()),
                    "unique_geometry_bytes": storage_bytes(list(result.values())),
                    "logical_array_span_bytes": sum(
                        a.nbytes for m in result.values() for a in m.values()
                    ),
                }
            )
        rows.append(
            {
                "source_id": programme.source.identifier,
                "support_controls": len(programme.supports),
                "entries": entries,
            }
        )
    output.mkdir(parents=True, exist_ok=True)
    (output / "costs.json").write_text(
        json.dumps(
            proof
            | {
                "cases": rows,
                "load_start": load,
                "load_end": os.getloadavg(),
                "logical_cpus": os.cpu_count(),
                "scope": "Normal-priority shared-host first/21 subsequent calls plus separate traced peak. Same complete111-component construction and standard UV/normal/material contract; independent support solids/metric UVs checked, UV charts need not be identical. Source/classification/planning, CSG union, smooth draw normals and export excluded. Final array counts/bytes are not all native allocations, copied traffic, cache or GPU/frame/device evidence.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    costs(args.output)
