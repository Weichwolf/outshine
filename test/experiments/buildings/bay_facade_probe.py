"""Same-source ribbon/bay elevations and actual near/opposite/whole comparisons."""

import argparse
import hashlib
import json
from dataclasses import asdict
from pathlib import Path

import numpy as np
import shapely
from bay_facade_access import check_routes
from bay_facade_mesh import expand
from bay_facade_plan import FIELD_MATERIAL, PROGRAMMES, RIM_MATERIAL, plan_bays
from bay_wall_domains import domains
from demand import View
from evidence import revision
from industrial_relief_probe import camera
from podium_probe import write
from shapely.affinity import translate
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import ROOF_MATERIAL, plan
from terraced_facade_prepare import positions
from terraced_facade_probe import materials
from terraced_facade_surface import joined
from terraced_mass_sources import sources
from window_glass_probe import views
from window_shade_probe import fingerprints


def palette():
    return materials() | {
        str(ROOF_MATERIAL): {
            "color.r": 0.26,
            "color.g": 0.29,
            "color.b": 0.30,
            "roughness": 0.84,
            "metallic": 0.0,
        },
        str(FIELD_MATERIAL): {
            "color.r": 0.075,
            "color.g": 0.11,
            "color.b": 0.13,
            "roughness": 0.60,
            "metallic": 0.18,
        },
        str(RIM_MATERIAL): {
            "color.r": 0.64,
            "color.g": 0.65,
            "color.b": 0.62,
            "roughness": 0.72,
            "metallic": 0.08,
        },
    }


def near(planned, old_window):
    candidates = [c for c in planned.cells if len(c.window_ids) >= 4]
    candidates = candidates or list(planned.cells)
    cell = next((c for c in candidates if old_window in c.window_ids), candidates[0])
    layer = cell.layers[-1]
    x0, z0, x1, z1 = layer.region.bounds
    tangent = np.asarray(layer.tangent)
    outward = np.array([tangent[1], -tangent[0]])
    xy = (
        np.asarray(layer.origin_xy)
        + tangent * ((x0 + x1) / 2)
        + outward * layer.depth_m
    )
    target = np.r_[xy, layer.base_m + (z0 + z1) / 2]
    eye = target + np.r_[outward * 8.8 - tangent * 0.9, 1.0]
    return cell.identifier, camera(
        View(eye, target, width=1920, height=1080, fov_deg=42)
    )


def diagram(output, name, terrace, programmes):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from building_plan_views import patch

    prepared = terrace.prepared
    walls, _rejected = domains(prepared, portals=terrace.portals)
    common = set.intersection(*({c.plane_id for c in p.cells} for p in programmes))
    wall = max(
        (w for w in walls if w.identifier in common),
        key=lambda w: shapely.from_wkb(w.region_wkb).area,
    )
    origin = positions(np.array([[*wall.origin_xy, 0.0]]), prepared.mass.core)[0, :2]
    core = prepared.mass.core
    tangent = wall.tangent[0] * np.asarray(core.across_xy) + wall.tangent[
        1
    ] * np.asarray(core.along_xy)
    fig, axes = plt.subplots(1, 2, figsize=(19.2, 10.8))
    for axis, programme in zip(axes, programmes, strict=True):
        patch(
            axis,
            shapely.from_wkb(wall.region_wkb),
            facecolor="#e1e0da",
            edgecolor="#666666",
            linewidth=0.4,
        )
        for cell in programme.cells:
            if cell.plane_id != wall.identifier:
                continue
            for layer in cell.layers:
                region = translate(
                    layer.region,
                    xoff=float((np.asarray(layer.origin_xy) - origin) @ tangent),
                    yoff=layer.base_m,
                )
                color = "#344c56" if layer.material == FIELD_MATERIAL else "#a4aaa2"
                patch(axis, region, facecolor=color, edgecolor="#263135", linewidth=0.4)
        axis.set(
            aspect="equal",
            xlabel="Actual wall U (m)",
            ylabel="Source height (m)",
            title=programme.programme,
        )
        axis.autoscale_view()
    fig.suptitle(wall.identifier + " · same openings and protected routes")
    fig.tight_layout()
    fig.savefig(output / (name + "-elevation.png"), dpi=100)
    plt.close(fig)


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the facade composition producer first")
    output.mkdir(parents=True, exist_ok=True)
    records = []
    for index, kind in ((0, "cascade"), (3, "paired-wings")):
        row = sources()[index]
        owner = RoofedProducts(plan(row, kind))
        base = owner.draw(True)
        hashes = fingerprints(base)
        terrace = owner.body.plan
        programmes = [
            plan_bays(terrace.prepared, style, portals=terrace.portals)
            for style in PROGRAMMES
        ]
        prefix = f"Bay{index}-{kind}"
        diagram(output, prefix, terrace, programmes)
        old_window, poses = views(owner, base)
        diagnostic_cell, poses["Near"] = near(programmes[1], old_window)
        poses = {key: poses[key] for key in ("Near", "Whole")}
        from stepped_source_probe import overview

        poses["Opposite"] = camera(overview(base["vertices"], [-1, 1.25, 0.65]))
        for variant, prepared in [
            ("Before", None),
            *zip(PROGRAMMES, programmes, strict=True),
        ]:
            name = prefix + "-" + variant
            addition = None if prepared is None else expand(prepared)[0]
            mesh = base if addition is None else joined((base, addition))
            for key, original in base.items():
                if not np.array_equal(mesh[key][: len(original)], original):
                    raise ValueError(
                        "Facade composition changed the original draw prefix"
                    )
            plan_path = output / (name + "-plan.json")
            plan_path.write_text(
                json.dumps(
                    asdict(prepared) if prepared else None,
                    default=lambda value: value.hex(),
                    indent=2,
                )
                + "\n"
            )
            record = {
                "source_id": row["id"],
                "source_properties": row["properties"],
                "source_footprint_wkb": row["polygon"].wkb_hex,
                "camera": poses["Whole"],
                "diagnostic_bay": diagnostic_cell,
                "source": {"material_definitions": palette()},
                "facade_materials": {},
                "ground_span_m": 10000,
                "original_arrays_sha256": hashes,
                "original_triangles": len(base["faces"]),
                "added_triangles": 0 if addition is None else len(addition["faces"]),
                "bay_plan": str(plan_path),
                "bay_plan_sha256": hashlib.sha256(plan_path.read_bytes()).hexdigest(),
                "route_check": None
                if prepared is None
                else check_routes(prepared, terrace.prepared, terrace.portals),
                "scope": "Existing coupled source body/roof/openings; additive coherent facade cells. Original arrays exact prefix; same cameras/light/source glass. Explicit outer products, no native/controller/whole-city/cache/automatic shadow or radiance LOD admission.",
            }
            write(output, name, mesh, record, proof)
            saved = json.loads((output / (name + "-geometry.json")).read_text())
            for pose, value in poses.items():
                label = name + pose
                path = output / (label + "-geometry.json")
                path.write_text(
                    json.dumps(saved | {"city": label, "camera": value}, indent=2)
                    + "\n"
                )
                records.append(
                    {"name": label, "geometry": str(path), "source_id": row["id"]}
                )
    (output / "models.json").write_text(
        json.dumps(proof | {"records": records}, indent=2) + "\n"
    )
    print(json.dumps({"models": len(records), "output": str(output)}), flush=True)


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
