"""Same-source 2D profiles and actual near/opposite/whole additive shade controls."""

import argparse
import hashlib
import json
from dataclasses import asdict
from pathlib import Path

import numpy as np
from evidence import revision
from industrial_relief_probe import camera
from outshine_ref.buildings.rooflight_panels import triangle_surface
from podium_probe import write
from stepped_source_probe import overview
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import ROOF_MATERIAL, plan
from terraced_facade_probe import materials
from terraced_facade_surface import joined
from terraced_mass_sources import sources
from window_glass_probe import views
from window_shade_mesh import SHADE_MATERIAL, expand, profile
from window_shade_plan import PROGRAMMES, plan_shades


def fingerprints(mesh):
    return {k: hashlib.sha256(v.tobytes()).hexdigest() for k, v in mesh.items()}


def diagram(output, plans, name):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(2, len(plans), figsize=(12, 7), squeeze=False)
    for column, item in enumerate(plans):
        slot = item.slots[0]
        xy = np.asarray(profile(slot).exterior.coords)
        axes[0, column].fill(xy[:, 0], xy[:, 1], color="#424d57")
        axes[0, column].set_title(item.programme + ": front section")
        axes[0, column].set_xlabel("along facade (m)")
        axes[0, column].set_ylabel("above component base (m)")
        mesh = expand(item, [slot.identifier])
        xyz = mesh["vertices"][mesh["faces"]]
        relative = xyz[:, :, :2] - np.asarray(slot.centre)
        along = relative @ np.asarray(slot.tangent)
        inward = relative @ np.array([-slot.tangent[1], slot.tangent[0]])
        from mpl_toolkits.mplot3d.art3d import Poly3DCollection

        axes[1, column].remove()
        axis = fig.add_subplot(2, len(plans), len(plans) + column + 1, projection="3d")
        points = np.stack((along, -inward, xyz[:, :, 2] - slot.bottom_m), axis=2)
        axis.add_collection3d(
            Poly3DCollection(
                points, facecolor="#798a97", edgecolor="#27313a", linewidth=0.3
            )
        )
        flat = points.reshape(-1, 3)
        axis.auto_scale_xyz(flat[:, 0], flat[:, 1], flat[:, 2])
        axis.set(xlabel="along (m)", ylabel="outward (m)", zlabel="height (m)")
        axis.set_box_aspect(np.ptp(flat, axis=0))
    fig.tight_layout()
    fig.savefig(output / (name + "-profiles.png"), dpi=180)
    plt.close(fig)


def models(output, cases):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the shade model producer first")
    output.mkdir(parents=True, exist_ok=True)
    records = []
    for index, kind in cases:
        row = sources()[index]
        owner = RoofedProducts(plan(row, kind))
        base = owner.draw(True)
        original = fingerprints(base)
        prepared = owner.body.facade.prepared
        _window, poses = views(owner, base)
        poses = {key: poses[key] for key in ("Near", "Whole")}
        poses["Opposite"] = camera(overview(base["vertices"], [-1, 1.25, 0.65]))
        palette = materials() | {
            str(ROOF_MATERIAL): {
                "color.r": 0.26,
                "color.g": 0.29,
                "color.b": 0.30,
                "roughness": 0.84,
                "metallic": 0.0,
            },
            str(SHADE_MATERIAL): {
                "color.r": 0.085,
                "color.g": 0.10,
                "color.b": 0.11,
                "roughness": 0.55,
                "metallic": 0.25,
            },
        }
        plans = [plan_shades(prepared, style) for style in PROGRAMMES]
        diagram(output, plans, f"Shade{index}-{kind}")
        for variant, shade_plan in [
            ("Before", None),
            *zip(PROGRAMMES, plans, strict=True),
        ]:
            addition = None if shade_plan is None else expand(shade_plan)
            mesh = (
                base
                if addition is None
                else joined((base, triangle_surface(addition, SHADE_MATERIAL)))
            )
            if fingerprints(base) != original:
                raise ValueError("Shade expansion changed its source draw")
            if addition is not None:
                for key in base:
                    if not np.array_equal(mesh[key][: len(base[key])], base[key]):
                        raise ValueError(
                            "Shade composition changed the original draw prefix"
                        )
            name = f"Shade{index}-{kind}-{variant}"
            record = {
                "source_id": row["id"],
                "source_properties": row["properties"],
                "source_footprint_wkb": row["polygon"].wkb_hex,
                "source": {"material_definitions": palette},
                "facade_materials": {},
                "ground_span_m": 10000,
                "camera": poses["Whole"],
                "original_arrays_sha256": original,
                "original_triangles": len(base["faces"]),
                "shade_plan": None if shade_plan is None else asdict(shade_plan),
                "added_triangles": 0 if addition is None else len(addition["faces"]),
                "scope": "Additive shades on existing coupled source body/roof/openings; source draw prefix exact. Same cameras/daylight/source glass. Full detail diagnostic, no neighbour reservations, structural load, native/controller or radiance/LOD admission.",
            }
            write(output, name, mesh, record, proof)
            saved = json.loads((output / (name + "-geometry.json")).read_text())
            for pose, view in poses.items():
                label = name + pose
                (output / (label + "-geometry.json")).write_text(
                    json.dumps(saved | {"city": label, "camera": view}, indent=2) + "\n"
                )
                records.append(
                    {
                        "name": label,
                        "source_id": row["id"],
                        "geometry": str(output / (label + "-geometry.json")),
                        "triangles": len(mesh["faces"]),
                    }
                )
    (output / "models.json").write_text(
        json.dumps(proof | {"records": records}, indent=2) + "\n"
    )
    print(json.dumps(records), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--case", choices=("simple", "courtyard", "both"), default="both"
    )
    args = parser.parse_args()
    output = args.output.resolve()
    if output == Path("/tmp") or not output.is_relative_to(Path("/tmp")):
        parser.error("--output requires an explicit subdirectory of /tmp")
    cases = {
        "simple": [(0, "cascade")],
        "courtyard": [(3, "paired-wings")],
        "both": [(0, "cascade"), (3, "paired-wings")],
    }
    models(output, cases[args.case])


if __name__ == "__main__":
    main()
