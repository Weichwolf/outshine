"""Matched complete/near roofs and explicitly clipped physical diagnostic solids."""

import numpy as np
from demand import View
from industrial_relief_probe import camera
from outshine_ref.buildings.rooflight_panels import triangle_surface
from podium_probe import write
from structural_shell_geometry import native
from terraced_facade_prepare import positions
from terraced_facade_probe import materials
from terraced_facade_surface import joined
from tile_terraced_crowns import crown_definitions


def cutaway(owner):
    body, crown = owner.plan.body, owner.plan.crown
    floors = owner.body.facade.physical().floors
    components = [(floors.shell, 12), (floors.stairs, 5)]
    components += [
        (p, d.volume.material)
        for p, d in zip(owner.physical_roof(), crown.parts, strict=True)
    ]
    across, along = np.asarray(body.core.across_xy), np.asarray(body.core.along_xy)
    origin = np.asarray(body.core.origin_xy)
    planes = [
        (np.r_[across, 0], float(origin @ across) + 0.01),
        (np.r_[-across, 0], -float(origin @ across) - 4.0),
        (np.r_[along, 0], float(origin @ along) - 1.0),
        (np.r_[-along, 0], -float(origin @ along) - 5.0),
        (np.array([0, 0, 1]), crown.deck_m - 5.50),
    ]
    meshes = []
    for product, material in components:
        solid = native(product.mesh, product.vertex_roots)
        for normal, offset in planes:
            solid = solid.trim_by_plane(normal, offset)
        if not solid.is_empty():
            raw = solid.to_mesh64()
            meshes.append(
                triangle_surface(
                    {
                        "vertices": np.asarray(raw.vert_properties)[:, :3],
                        "faces": np.asarray(raw.tri_verts),
                    },
                    material,
                )
            )
    return joined(tuple(meshes))


def export(owners, output, proof):
    current = owners["access"]
    core, deck = current.plan.body.core, current.plan.crown.deck_m
    palette = materials() | {"60003": crown_definitions()["60003"]}
    queue = []
    for mode, owner in owners.items():
        for view_name, mesh, eye, target in (
            ("roof", owner.draw(True), (7, -9, deck + 6), (1.5, 2, deck + 0.5)),
            ("cutaway", cutaway(owner), (-7, -8, deck + 2), (1.5, 2, deck - 1.5)),
        ):
            view = View(
                *positions(np.array([eye, target]), core),
                width=1920,
                height=1080,
                fov_deg=48,
            )
            name = mode + "-" + view_name
            write(
                output,
                name,
                mesh,
                {
                    "camera": camera(view),
                    "source_id": owner.plan.body.identifier,
                    "source_properties_changed": False,
                    "source": {"material_definitions": palette},
                    "facade_materials": {},
                    "ground_span_m": 10000,
                    "scope": "Actual complete near draw"
                    if view_name == "roof"
                    else "Explicit clipped actual physical last-two-storey/core solids; clipping caps are diagnostic, not product surfaces",
                },
                proof,
            )
            queue.append(
                {
                    "geometry": str(output / (name + "-geometry.json")),
                    "output": str(output / "images"),
                }
            )
    return queue
