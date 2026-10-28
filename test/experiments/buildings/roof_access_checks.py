"""Independent actual-solid and finite-actor checks of the roof extension."""

import numpy as np
import shapely
from structural_shell_geometry import NativeColumns, inspect_poses
from terraced_crown_probe import roof_checks
from terraced_facade_prepare import positions
from terraced_mass_admission import inspect, stair_columns
from terraced_terrace_access import continuous
from structural_shell_geometry import native


def check(owner):
    access = owner.plan.access
    if access is None:
        raise ValueError("Roof access checks require an admitted extension")
    previous = inspect(owner.body)
    roof = owner.physical_roof()
    poses = positions(np.asarray(access.route_xyz), owner.plan.body.core)
    columns, extras = stair_columns(owner.body, poses)
    columns.components += tuple(
        NativeColumns(
            native(p.mesh, p.vertex_roots),
            owner.plan.body.bottom_m - 1,
            owner.plan.source_top_m + 1,
        )
        for p in roof
    )
    clearance = inspect_poses(columns, poses)
    floors = owner.body.facade.physical().floors
    parts = [
        dict(p.mesh, vertex_roots=p.vertex_roots)
        for p in (floors.shell, floors.stairs, floors.landing, *roof)
    ]
    route = shapely.LineString(poses[-2:, :2])
    passage = continuous(parts, route, owner.plan.crown.deck_m)
    return {
        "existing_body": previous,
        "roof": roof_checks(owner),
        "roof_stair_poses": len(poses),
        "sampled_clearance_m": clearance,
        "additional_collision_components": extras,
        "continuous_roof_door_route": passage,
        "scope": "Closed source-height components, independent sections, sampled stair foot disks/torso and continuous horizontal doorway route. No continuous stair controller, native World or device acceptance.",
    }
