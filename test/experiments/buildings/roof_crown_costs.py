"""Equal occupied/material surfaces and exact warm bindings, with scoped host work."""

import argparse
import json
import os
from pathlib import Path

import numpy as np
import shapely
from evidence import revision
from facade_parts import append
from outshine_ref.buildings.modelling.heightfield import heightfield_envelope
from provider_contract import attributes
from roof_crown_mesh import envelope, upper
from roof_crown_plan import plan
from roof_crown_present import body, skin
from roof_crown_sources import sources
from rooflight_lamina_probe import measured
from tile_roof_crowns import prepared
from verify_roof_envelopes import measures


def flatten_full_top(crown):
    top = upper(crown)
    top["vertices"][:, 2] = crown.top_m
    top["normals"][:] = [0, 0, 1]
    top["uv"] = top["vertices"][:, :2].copy()
    return heightfield_envelope(top, base_m=0.0, wall_material=1, base_material=0)


def bind(mesh):
    result = {
        k: v for k, v in mesh.items() if k not in ("parts", "groups", "face_tag_mask")
    }
    result["materials"] = result["materials"].copy()
    result["materials"][result["materials"] == 1] = 12
    return result


def equivalent(a, b):
    first, second = measures(a), measures(b)
    np.testing.assert_allclose(
        [first.volume(), first.surface_area()],
        [second.volume(), second.surface_area()],
        rtol=0,
        atol=1e-7,
    )
    for material in (9, 10):
        regions = []
        for e in (a, b):
            mask = (e.surface_roles == 0) & (e.mesh["materials"] == material)
            xy = e.mesh["vertices"][e.mesh["faces"][mask], :2]
            regions.append(shapely.union_all(shapely.polygons(xy)))
        if regions[0].symmetric_difference(regions[1]).area > 1e-8:
            raise ValueError("Coarse alternatives differ in material coverage")


def draw(body_mesh, skin_mesh):
    return append(bind(body_mesh), [bind(skin_mesh)])


def rebuild_draw(crown):
    env = envelope(crown, coarse=True)
    return draw(body(crown), skin(env, crown.deck_m))


def costs(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze cost experiment first")
    load = os.getloadavg()
    records = []
    for index, (row, _neighbors) in enumerate(sources()):
        crown = plan(
            row["polygon"],
            attributes(row["properties"])["buildingHeight"],
            "parapet-service",
        )
        asset = prepared(crown)
        full_top_count = len(upper(crown)["faces"])
        direct = envelope(crown, coarse=True)
        alternative = flatten_full_top(crown)
        equivalent(direct, alternative)
        entries = []
        for method, call in (
            ("direct-coarse", lambda: envelope(crown, coarse=True)),
            ("flatten-full-top", lambda: flatten_full_top(crown)),
        ):
            value, timing = measured(call)
            entries.append(
                timing
                | {
                    "method": method,
                    "output_triangles": len(value.mesh["faces"]),
                    "upper_triangle_work": int(np.sum(value.surface_roles == 0))
                    if method == "direct-coarse"
                    else full_top_count,
                    "output_array_bytes": sum(v.nbytes for v in value.mesh.values()),
                    "output_array_count": len(value.mesh),
                }
            )
        for method, call in (
            ("reuse-body-skin-draw", lambda: draw(asset.body, asset.coarse_skin)),
            ("rebuild-body-skin-draw", lambda: rebuild_draw(crown)),
        ):
            value, timing = measured(call)
            expected = draw(asset.body, asset.coarse_skin)
            for key in expected:
                np.testing.assert_array_equal(value[key], expected[key])
            entries.append(
                timing
                | {
                    "method": method,
                    "output_triangles": len(value["faces"]),
                    "output_array_bytes": sum(v.nbytes for v in value.values()),
                    "new_output_array_bytes": sum(v.nbytes for v in value.values()),
                    "new_output_array_count": len(value),
                }
            )
        pair, timing = measured(lambda: prepared(crown))
        entries.append(
            timing
            | {
                "method": "prepare-both",
                "resident_mesh_array_bytes": sum(
                    v.nbytes for e in (pair.full, pair.coarse) for v in e.mesh.values()
                )
                + sum(
                    v.nbytes
                    for mesh in (pair.body, pair.full_skin, pair.coarse_skin)
                    for v in mesh.values()
                ),
                "resident_topology_array_bytes": sum(
                    v.nbytes
                    for e in (pair.full, pair.coarse)
                    for v in (e.vertex_roots, e.source_triangles, e.surface_roles)
                ),
                "scope": "Already chosen/partitioned logical crown; source/context/choice preparation excluded",
            }
        )
        records.append(
            {"index": index, "id": row["id"], "kind": crown.kind, "entries": entries}
        )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(
            proof
            | {
                "records": records,
                "logical_cpus": os.cpu_count(),
                "load_before": load,
                "load_after": os.getloadavg(),
                "scope": "Host Python construction/binding,21 warm calls plus separate traced call; traced Python/NumPy peak excludes GEOS/native heap, process and renderer. Alternatives preserve occupied/material surfaces with metric UVs; chart orientation differs, no textured/anisotropic shader-equivalence claim. Full and coarse resident assets reported separately; no serialization, warm load, native/device/frame admission.",
            },
            indent=2,
        )
        + "\n"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path("/tmp").resolve()):
        parser.error("--output must be inside /tmp")
    costs(args.output)


if __name__ == "__main__":
    main()
