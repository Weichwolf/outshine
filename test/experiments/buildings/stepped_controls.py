"""Negative controls query changed actual solids, rather than only declared room paths."""

import numpy as np
from boolean_building_bands import construct
from building_vertical import level, mesh
from opening_details import attach, expand, slots
from outshine_ref.buildings.modelling import PrismVolume, union_prisms
from stepped_envelope_geometry import declared as exterior_prisms
from stepped_envelope_geometry import geometry as complete_geometry
from stepped_source import layers
from structural_shell_geometry import (
    NativeColumns,
    assembly,
    inspect_poses,
    mesh_section,
    native,
)
from verify_structural_shell import access
from verify_terraces import check, circulation
from verify_vertical import connected_spaces, landing_poses


def rejected(label, operation):
    try:
        operation()
    except (ValueError, AssertionError) as error:
        return dict(case=label, status="rejected", reason=str(error))
    raise AssertionError("Broken actual stepped product accepted: " + label)


def separate_caps(plan):
    values, _, _ = exterior_prisms(plan)
    pieces = [union_prisms([value]) for value in values]
    vertices = []
    faces = []
    roots = []
    offset = 0
    for piece in pieces:
        vertices.append(piece.mesh["vertices"])
        faces.append(piece.mesh["faces"] + offset)
        roots.append(piece.vertex_roots + offset)
        offset += len(piece.mesh["vertices"])
    from types import SimpleNamespace

    return SimpleNamespace(
        mesh=dict(vertices=np.concatenate(vertices), faces=np.concatenate(faces)),
        vertex_roots=np.concatenate(roots),
    )


def negative(plan):
    product = attach(mesh(plan), expand(slots(plan)))
    check(plan, product)
    circulation(plan, product)
    shell = construct(plan)
    _, collision = assembly(plan, shell, frames=True)
    results = []
    for label, material in (("missing-deck", 4), ("missing-parapet", 7)):
        selected = product["materials"] != material
        bad = {
            key: value if key == "vertices" else value[selected]
            for key, value in product.items()
        }
        results.append(rejected(label, lambda: check(plan, bad)))
    record = plan["terrace_access"][0]
    current = level(plan, record["storey"])
    door = next(
        door
        for door in current["doors"]
        if door["kind"] == "terrace"
        and door["id"] == "terrace-" + str(record["component"])
    )
    blocker = union_prisms(
        [
            PrismVolume(
                "sealed-terrace-portal",
                door["polygon"],
                current["floor_z_m"],
                current["floor_z_m"] + door["height_m"],
            )
        ]
    )
    blocked = collision + native(blocker.mesh, blocker.vertex_roots)
    poses = landing_poses(np.asarray(record["path"]), current["floor_z_m"])
    results.append(
        rejected(
            "sealed-terrace-portal",
            lambda: inspect_poses(
                NativeColumns(blocked, -1.0, plan["source_top_m"] + 1), poses
            ),
        )
    )
    first = level(plan, 1)
    cap = union_prisms(
        [
            PrismVolume(
                "closed-upper-stair-opening",
                first["footprint"],
                first["base_z_m"],
                first["floor_z_m"],
            )
        ]
    )
    results.append(
        rejected(
            "closed-upper-stair-opening",
            lambda: access(plan, collision + native(cap.mesh, cap.vertex_roots)),
        )
    )
    disconnected = plan | dict(
        storey_portals=[
            [r for r in rows if r["id"] != record["door_id"]]
            for rows in plan["storey_portals"]
        ]
    )
    results.append(
        rejected("unreachable-terrace-graph", lambda: connected_spaces(disconnected))
    )
    declared = layers(plan)
    incorrect = []
    for index, source in enumerate(declared):
        high = (
            level(plan, index + 1)["base_z_m"]
            if index + 1 < len(declared)
            else plan["body_top_m"]
        )
        low = plan["base_z_m"] if index == 0 else level(plan, index)["base_z_m"]
        incorrect.append(PrismVolume(source.identifier, source.polygon, low, high))
    old = union_prisms(incorrect)
    height = current["floor_z_m"] - 0.01
    section = mesh_section(old.mesh, old.vertex_roots, height)
    if current["terrace"].intersection(section).area > 0.000001:
        raise AssertionError(
            "Lowered far terrace negative control no longer exposes missing deck"
        )
    results.append(
        dict(
            case="lowered-far-terrace",
            status="rejected",
            missing_deck_m2=current["terrace"].area,
        )
    )
    separate = separate_caps(plan)
    results.append(
        rejected(
            "internal-body-and-guard-caps", lambda: complete_geometry(plan, separate)
        )
    )
    return results
