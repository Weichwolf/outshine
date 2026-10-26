"""Independent physical checks for one proposed source mass; no runtime admission."""

from itertools import pairwise

import numpy as np
import shapely
from building_plan import FLOOR_M
from outshine_ref.buildings.modelling import PrismVolume
from structural_shell_geometry import NativeColumns, inspect_poses, mesh_section, native
from terraced_facade_access import check_arrival
from terraced_mass_collision import ComponentColumns
from terraced_mass_geometry import bands
from terraced_mass_rooms import cavities
from terraced_terrace_access import check, continuous, selected_parts


def closed(product):
    solid = native(product.mesh, product.vertex_roots)
    xyz = product.mesh["vertices"]
    triangles = xyz[product.mesh["faces"]] - xyz.mean(axis=0)
    volume = (
        np.einsum(
            "ij,ij->i", triangles[:, 0], np.cross(triangles[:, 1], triangles[:, 2])
        ).sum()
        / 6
    )
    if (
        max(abs(solid.volume() - product.volume_m3), abs(volume - product.volume_m3))
        > 1e-7
    ):
        raise ValueError("Closed structural volume changed after actual mesh roundtrip")
    if abs(solid.surface_area() - product.area_m2) > 1e-7:
        raise ValueError("Closed structural area changed after actual mesh roundtrip")


def shell_sweep(owner):
    prepared, mass = owner.plan.prepared, owner.plan.prepared.local
    cuts = list(cavities(mass))
    cuts.extend(
        PrismVolume(
            w.identifier,
            shapely.from_wkb(w.cutter_wkb),
            w.bottom_m,
            w.bottom_m + w.height_m,
        )
        for w in (*prepared.facade.windows, *owner.plan.portals)
    )
    positive = bands(mass)
    shell = owner.facade.physical().floors.shell
    core = prepared.mass.core
    vertices = np.array(shell.mesh["vertices"], copy=True)
    vertices[:, :2] = (vertices[:, :2] - core.origin_xy) @ np.array(
        [core.across_xy, core.along_xy]
    ).T
    mesh = dict(shell.mesh, vertices=vertices)
    heights = sorted({z for c in (*positive, *cuts) for z in (c.low_m, c.high_m)})
    volume, maximum = 0.0, 0.0
    for lo, hi in pairwise(heights):
        z = (lo + hi) / 2
        expected = shapely.union_all(
            [c.polygon for c in positive if c.low_m < z < c.high_m]
        )
        expected = expected.difference(
            shapely.union_all([c.polygon for c in cuts if c.low_m < z < c.high_m])
        )
        actual = mesh_section(mesh, shell.vertex_roots, z)
        error = expected.symmetric_difference(actual).area
        maximum = max(maximum, error)
        if error > 1e-7:
            raise ValueError(f"Independent shell section changed by {error}m² at {z}m")
        volume += expected.area * (hi - lo)
    if abs(volume - shell.volume_m3) > 1e-7:
        raise ValueError(
            f"Independent shell sweep changed volume by {volume - shell.volume_m3}m³"
        )
    return {
        "independent_shell_slabs": len(heights) - 1,
        "maximum_shell_section_xor_m2": maximum,
        "shell_volume_error_m3": abs(volume - shell.volume_m3),
    }


def additions(owner):
    for component in owner.components:
        s, mesh = component.source, component.mesh
        body = native(mesh)
        height = s.high_m - s.low_m
        if abs(body.volume() - s.polygon.area * height) > 1e-7:
            raise ValueError("Added terrace component changed prism volume")
        if (
            abs(body.surface_area() - (2 * s.polygon.area + s.polygon.length * height))
            > 1e-7
        ):
            raise ValueError("Added terrace component changed prism area")
        if mesh["vertices"][:, 2].max() > owner.plan.prepared.mass.top_m:
            raise ValueError("Added terrace component exceeds source height")
    frames = owner.facade.physical().frames
    for part in np.unique(frames["parts"]):
        rows = np.flatnonzero(frames["parts"] == part)
        if native(frames, rows=rows).volume() <= 0:
            raise ValueError("Facade component has no closed positive volume")
    return {
        "closed_terrace_components": len(owner.components),
        "closed_facade_components": len(np.unique(frames["parts"])),
    }


def stair_columns(owner, poses):
    mass, asset = owner.plan.prepared.mass, owner.facade.physical()
    low, high = mass.bottom_m - 1, mass.top_m + 1
    columns = ComponentColumns(asset.floors, low, high)
    minimum, maximum = (
        poses.min(axis=0) - [0.31, 0.31, 1e-7],
        poses.max(axis=0) + [0.31, 0.31, 1.90],
    )
    extras = [c.mesh for c in owner.components]
    frames = asset.frames
    triangles = frames["vertices"][frames["faces"]]
    hit = np.all(triangles.min(axis=1) <= maximum, axis=1) & np.all(
        triangles.max(axis=1) >= minimum, axis=1
    )
    for part in np.unique(frames["parts"][hit]):
        extras.append(
            {
                "vertices": frames["vertices"],
                "faces": frames["faces"][frames["parts"] == part],
            }
        )
    selected = []
    for mesh in extras:
        xyz = mesh["vertices"][mesh["faces"]]
        if np.all(xyz.min(axis=(0, 1)) <= maximum) and np.all(
            xyz.max(axis=(0, 1)) >= minimum
        ):
            selected.append(NativeColumns(native(mesh), low, high))
    columns.components += tuple(selected)
    return columns, len(selected)


def inspect(owner):
    mass, asset = owner.plan.prepared.mass, owner.facade.physical()
    for name in ("shell", "stairs", "landing"):
        closed(getattr(asset.floors, name))
    result = shell_sweep(owner) | additions(owner)
    poses = np.concatenate(
        [
            np.asarray(mass.core.path) + [0, 0, i * mass.core.rise_m]
            for i in range(len(mass.storeys) - 1)
        ]
    )
    columns, extra_count = stair_columns(owner, poses)
    clearance = inspect_poses(columns, poses)
    for portal in owner.plan.portals:
        if portal.bottom_m < mass.bottom_m or portal.bottom_m >= mass.top_m:
            raise ValueError("Portal leaves the supplied source interval")
    arrival = check_arrival(mass, asset)
    approach = shapely.from_wkb(mass.core.approach_wkb)
    ground = continuous(
        selected_parts(owner, approach, mass.bottom_m + FLOOR_M),
        approach,
        mass.bottom_m + FLOOR_M,
    )
    terraces = check(owner)
    return result | {
        "sampled_stair_poses": len(poses),
        "sampled_stair_clearance_m": clearance,
        "additional_stair_collision_components": extra_count,
        "arrival": arrival,
        "ground_approach": ground,
        "terraces": terraces,
        "scope": "Closed physical parts, independent logical shell sweep, sampled stairs including possible facade/terrace interference, continuous ground arrival/approach and declared terrace routes. No continuous stair/controller/free-roaming/cache/runtime/device admission.",
    }
