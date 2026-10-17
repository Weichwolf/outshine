"""Paired actual body/roof sources on a flat plane; all outputs explicitly in /tmp."""

import argparse
import json
from itertools import pairwise
from pathlib import Path

import numpy as np
import shapely
from evidence import revision
from industrial_relief_probe import camera
from podium_probe import write
from stepped_source_probe import overview
from structural_shell_geometry import mesh_section, native
from terraced_admission_probe import controls
from terraced_crown_geometry import ROOF_MATERIAL, RoofedProducts
from terraced_crown_plan import plan
from terraced_facade_probe import materials
from terraced_mass_admission import inspect
from terraced_mass_plan import plan as mass_plan
from terraced_mass_sources import sources
from terraced_terrace_geometry import TerraceProducts
from terraced_terrace_plan import plan as terrace_plan


def cases():
    real = sources()
    service = next(r for r in controls() if r["id"].endswith("/121"))
    return [
        (real[0], "cascade"),
        (real[3], "paired-wings"),
        (service, service["programme"]),
    ]


def roof_checks(owner):
    products = owner.physical_roof()
    if products is None:
        raise ValueError("Actual roof control requires a crown")
    body, crown = owner.plan.body, owner.plan.crown
    volume, volume_error, area_error = 0.0, 0.0, 0.0
    local_meshes = []
    for descriptor, product in zip(crown.parts, products, strict=True):
        mesh, source = product.mesh, descriptor.volume
        solid = native(mesh, product.vertex_roots)
        centred = mesh["vertices"][mesh["faces"]] - mesh["vertices"].mean(axis=0)
        signed = (
            np.einsum(
                "ij,ij->i", centred[:, 0], np.cross(centred[:, 1], centred[:, 2])
            ).sum()
            / 6
        )
        expected = source.polygon.area * (source.high_m - source.low_m)
        area = 2 * source.polygon.area + source.polygon.length * (
            source.high_m - source.low_m
        )
        volume_error = max(
            volume_error, abs(signed - expected), abs(solid.volume() - signed)
        )
        area_error = max(area_error, abs(solid.surface_area() - area))
        volume += signed
        vertices = np.array(mesh["vertices"], copy=True)
        if (
            vertices[:, 2].min() < body.top_m - 0.08 - 1e-9
            or vertices[:, 2].max() > owner.plan.source_top_m + 1e-9
        ):
            raise ValueError("Roof component leaves its opaque contact/source height")
        vertices[:, :2] = (vertices[:, :2] - body.core.origin_xy) @ np.array(
            [body.core.across_xy, body.core.along_xy]
        ).T
        local_meshes.append(dict(mesh, vertices=vertices))
    if max(volume_error, area_error) > 1e-7:
        raise ValueError(
            "Closed roof component disagrees with independent source integrals"
        )
    heights = sorted(
        {z for p in crown.parts for z in (p.volume.low_m, p.volume.high_m)}
    )
    maximum, union_volume = 0.0, 0.0
    for low, high in pairwise(heights):
        z = (low + high) / 2
        actual = shapely.union_all(
            [
                mesh_section(m, p.vertex_roots, z)
                for m, p in zip(local_meshes, products, strict=True)
            ]
        )
        expected = shapely.union_all(
            [
                p.volume.polygon
                for p in crown.parts
                if p.volume.low_m < z < p.volume.high_m
            ]
        )
        maximum = max(maximum, actual.symmetric_difference(expected).area)
        union_volume += expected.area * (high - low)
    footprint = mesh_section(
        products[0].mesh, products[0].vertex_roots, crown.deck_m - 0.04
    )
    error = footprint.symmetric_difference(
        shapely.from_wkb(body.storeys[-1].footprint_wkb)
    ).area
    if max(maximum, error, abs(union_volume - volume)) > 1e-7:
        raise ValueError("Actual roof sections, courtyard or disjoint volumes disagree")
    if (
        abs(
            max(p.mesh["vertices"][:, 2].max() for p in products)
            - owner.plan.source_top_m
        )
        > 1e-9
    ):
        raise ValueError("Actual roof changed the delivered upper height")
    return {
        "closed_volume_m3": volume,
        "footprint_xor_m2": error,
        "roof_triangles": sum(len(p.mesh["faces"]) for p in products),
        "closed_roof_components": len(products),
        "independent_volume_error_m3": volume_error,
        "independent_area_error_m2": area_error,
        "maximum_roof_section_xor_m2": maximum,
    }


def roof_figure(output, owners):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from shapely.plotting import plot_polygon

    figure, axes = plt.subplots(1, 3, figsize=(19.2, 10.8), layout="constrained")
    colours = {
        "slab": "#62757a",
        "wall": "#d9d6ca",
        "coping": "#333b42",
        "service": "#889599",
        "service-cap": "#889599",
    }
    for owner, axis in zip(owners, axes, strict=True):
        crown = owner.plan.crown
        for part in crown.parts:
            for polygon in shapely.get_parts(part.volume.polygon):
                plot_polygon(
                    polygon,
                    ax=axis,
                    add_points=False,
                    color=colours[part.role],
                    alpha=1,
                )
        axis.set_aspect("equal")
        axis.set_title(
            f"{crown.kind} / {len(owner.plan.body.storeys)} storeys\nSource top {crown.top_m:.3f}m / roof datum {crown.deck_m:.3f}m"
        )
        axis.set_xlabel("core across / m")
        axis.set_ylabel("core along / m")
    figure.savefig(output / "roof-plans.png", dpi=100)
    plt.close(figure)


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the coupled-roof producer first")
    output.mkdir(parents=True, exist_ok=True)
    palette = materials() | {
        str(ROOF_MATERIAL): {
            "color.r": 0.26,
            "color.g": 0.29,
            "color.b": 0.30,
            "roughness": 0.84,
            "metallic": 0.0,
        }
    }
    records, owners = [], []
    for index, (row, kind) in enumerate(cases()):
        owner = RoofedProducts(plan(row, kind))
        owners.append(owner)
        before_owner = TerraceProducts(terrace_plan(mass_plan(row, kind)))
        before, after, compact = (
            before_owner.draw(True),
            owner.draw(True),
            owner.draw(False),
        )
        checks = {"body": inspect(owner.body), "roof": roof_checks(owner)}
        view = overview(np.r_[before["vertices"], after["vertices"]], [1, -1.25, 0.85])
        record = {
            "source_id": row["id"],
            "source_properties": row["properties"],
            "source_footprint_wkb": row["polygon"].wkb_hex,
            "source": {"material_definitions": palette},
            "facade_materials": {},
            "ground_span_m": 10000,
            "camera": camera(view),
            "physical_checks": checks,
            "physical_check_product": "coupled After body/cap; Before uses the original separate body",
            "programme": kind,
            "roof_kind": owner.plan.crown.kind,
            "source_top_m": owner.plan.source_top_m,
            "body_top_m": owner.plan.body.top_m,
            "storeys": len(owner.plan.body.storeys),
            "roof_plan_attempts": owner.plan.attempts,
            "scope": "Same complete provider source and view, coupled highest roof/body/facades and simple open floors. Compact is an explicit direct position reference; no highest-roof access, general radiance/LOD/native/device admission.",
        }
        for label, mesh in (("Before", before), ("After", after), ("Compact", compact)):
            write(output, f"Crown-{index}-{label}", mesh, record, proof)
        records.append(
            {
                "id": row["id"],
                "roof": owner.plan.crown.kind,
                "before_triangles": len(before["faces"]),
                "after_triangles": len(after["faces"]),
                "compact_triangles": len(compact["faces"]),
                "checks": checks,
            }
        )
        print(
            json.dumps({k: v for k, v in records[-1].items() if k != "checks"}),
            flush=True,
        )
    roof_figure(output, owners)
    (output / "coupled-roofs.json").write_text(
        json.dumps(proof | {"records": records}, indent=2) + "\n"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    target = args.output.resolve()
    if target == Path("/tmp") or not target.is_relative_to(Path("/tmp")):
        parser.error("--output requires a subdirectory of /tmp")
    models(target)


if __name__ == "__main__":
    main()
