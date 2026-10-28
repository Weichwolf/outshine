"""Actual closed bay components, unchanged sources, finite routes and slab support."""

import math

import numpy as np
from structural_shell_geometry import mesh_section, native
from terraced_facade_prepare import positions
from terraced_mass_admission import inspect
from terraced_terrace_access import continuous, selected_parts, world_path


def component_geometry(facade):
    geometry = facade.recess_components
    mesh = geometry.mesh
    rows, errors, expected_volume = [], [], 0.0
    for recess in geometry.descriptors:
        for volume in recess.volumes:
            part = geometry.owners.index(volume.identifier)
            faces = mesh["faces"][mesh["parts"] == part]
            if not len(faces):
                raise ValueError(
                    "Missing actual recess component: " + volume.identifier
                )
            body = native({"vertices": mesh["vertices"], "faces": faces})
            height = volume.high_m - volume.low_m
            expected = volume.polygon.area * height
            area = 2 * volume.polygon.area + volume.polygon.length * height
            error = max(abs(body.volume() - expected), abs(body.surface_area() - area))
            if error > 1e-7:
                raise ValueError("Actual recess component changed volume/area")
            rows.append({"id": volume.identifier, "volume_m3": expected})
            errors.append(error)
            expected_volume += expected
    return {
        "closed_source_prisms": len(rows),
        "volume_m3": expected_volume,
        "maximum_prism_volume_or_area_error": max(errors),
        "components": rows,
    }


def reused_geometry(facade):
    prepared, components = facade.prepared, facade.recess_components
    coarse = facade.coarse
    if len(coarse.triangle_owners) != len(coarse.mesh["faces"]):
        raise ValueError("Recess compact ownership length changed")
    source = components.mesh
    for part, identifier in enumerate(components.owners):
        owner = coarse.owner_ids.index(identifier)
        actual = coarse.mesh["vertices"][
            coarse.mesh["faces"][coarse.triangle_owners == owner]
        ]
        wanted = source["vertices"][source["faces"][source["parts"] == part]]
        expected = positions(wanted.reshape(-1, 3), prepared.mass.core).reshape(
            -1, 3, 3
        )
        if actual.shape != expected.shape or not np.array_equal(actual, expected):
            raise ValueError("Actual compact recess component lost its source geometry")
    frames = facade.physical().frames
    count, face_count = len(source["vertices"]), len(source["faces"])
    if not np.array_equal(
        frames["vertices"][-count:], positions(source["vertices"], prepared.mass.core)
    ):
        raise ValueError("Physical recess positions differ from the shared assembly")
    offset = len(frames["vertices"]) - count
    if not np.array_equal(frames["faces"][-face_count:] - offset, source["faces"]):
        raise ValueError("Physical recess faces differ from the shared assembly")
    return {
        "shared_closed_and_compact_triangles": face_count,
        "shared_component_owners": len(components.owners),
    }


def actor_routes(owner):
    prepared, asset = owner.plan.prepared, owner.facade.physical()
    result = []
    for recess in prepared.recesses:
        routes = []
        for wkb in (recess.core_route_wkb, recess.door_route_wkb):
            route = world_path(owner.plan, wkb)
            checked = continuous(
                selected_parts(owner, route, recess.outer.bottom_m),
                route,
                recess.outer.bottom_m,
            )
            routes.append(checked)
        doorway = world_path(owner.plan, recess.door_route_wkb)
        factor = 1 / math.cos(math.pi / (4 * 32))
        feet = doorway.buffer(0.10 * factor, quad_segs=32)
        shell = asset.floors.shell
        below = mesh_section(
            shell.mesh, shell.vertex_roots, recess.outer.bottom_m - 0.08
        )
        missing = feet.difference(below).area
        if missing > 1e-8:
            raise ValueError("Actual recessed doorway lacks full slab foot support")
        result.append(
            {
                "id": recess.identifier,
                "routes": routes,
                "doorway_missing_foot_support_m2": missing,
            }
        )
    return result


def audit(owner, original_terraces):
    prepared = owner.plan.prepared
    original = original_terraces.prepared
    if prepared.mass != original.mass or prepared.local != original.local:
        raise ValueError("Recess recipe changed its original body/source/core")
    old = {w.identifier: w for w in original.facade.windows}
    new = {w.identifier: w for w in prepared.facade.windows}
    replaced = {r.source_window.identifier for r in prepared.recesses}
    if set(old) - set(new) != replaced or any(old[k] != w for k, w in new.items()):
        raise ValueError("Recess recipe changed an unrelated original window")
    if any(
        old[r.source_window.identifier] != r.source_window for r in prepared.recesses
    ):
        raise ValueError("Recess recipe lost original opening provenance")
    if owner.plan.portals != original_terraces.portals:
        raise ValueError("Recess recipe changed an existing terrace portal or path")
    return {
        "recesses": len(replaced),
        "retained_windows": len(new),
        "existing_body": inspect(owner),
        "extra_geometry": component_geometry(owner.facade),
        "shared_geometry": reused_geometry(owner.facade),
        "new_actor_routes": actor_routes(owner),
        "scope": "Original mass/core/source and all former portals retained, closed actual added prisms, exact shared compact/physical geometry, continuous declared actor tubes and full doorway foot support. No door-state/controller/whole-room/free-roaming/native/World/device admission.",
    }
