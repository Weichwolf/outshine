"""One source-stable stepped plan supplies complete near structure and exterior envelope."""

import time

import numpy as np
import shapely
from boolean_building_bands import construct
from building_terraces import PARAPET_HEIGHT_M
from building_vertical import build, level
from building_vertical import mesh as detailed_mesh
from interior_light_plan import lights
from opening_details import attach, expand, slots
from outshine_ref.buildings.modelling import PrismVolume, union_prisms
from stepped_form_plan import choose
from structural_shell_geometry import NativeColumns, assembly, inspect_poses, native
from tile_room_details import corner_buffers, numeric_properties
from verify_structural_shell import access
from verify_terraces import circulation
from verify_vertical import landing_poses


def planned(row):
    height = float(row["properties"]["render_height"])
    began = time.perf_counter()
    plan = build(
        dict(id=row["id"], polygon=row["polygon"], properties=numeric_properties(row)),
        body_top_m=height - PARAPET_HEIGHT_M,
    )
    if plan.get("vertical_status") != "connected-storeys":
        raise ValueError("No complete stepped base room/storey plan")

    def detail_clearance(current):
        from stepped_envelope_geometry import geometry

        _, _, complete, _ = exterior(current, 1)
        geometry(current, complete)
        near(current, 1)

    result = choose(plan, validate=detail_clearance)
    result["storey_portals"] = [list(row) for row in result["storey_portals"]]
    result["terrace_space_ids"] = []
    for record in result["terrace_access"]:
        index = record["storey"]
        identifier = f"{result['id']}:floor-{index}:terrace-{record['component']}"
        result["terrace_space_ids"].append(identifier)
        result["storey_portals"][index].append(
            dict(
                id=record["door_id"],
                rooms=[record["room_id"], identifier],
                outside=False,
            )
        )
    result["form_recipe"]["planning_ms"] = (time.perf_counter() - began) * 1000
    return result


def plan_for(row):
    try:
        with np.errstate(divide="raise", invalid="raise", over="raise"):
            return planned(row)
    except (FloatingPointError, RuntimeWarning) as error:
        raise ValueError(
            "Source stepped planning rejected numeric geometry: " + str(error)
        ) from error


def layers(plan, wall_material=1):
    result = []
    for index in range(plan["storeys"]):
        current = level(plan, index)
        high = (
            level(plan, index + 1)["floor_z_m"]
            if index + 1 < plan["storeys"]
            else plan["body_top_m"]
        )
        low = plan["base_z_m"] if index == 0 else current["floor_z_m"]
        result.append(
            PrismVolume(
                plan["id"] + f"/floor-{index}",
                shapely.set_precision(current["footprint"], 0.0),
                low,
                high,
                wall_material,
            )
        )
    return result


def roof_volume(plan, wall_material):
    top = level(plan, plan["storeys"] - 1)["footprint"]
    guard = top.difference(top.buffer(-0.12, join_style="mitre"))
    return PrismVolume(
        plan["id"] + "/roof-guard",
        shapely.set_precision(guard, 0.0),
        plan["body_top_m"],
        plan["source_top_m"],
        wall_material,
    )


def roof_guard(plan, wall_material):
    return union_prisms([roof_volume(plan, wall_material)])


def terrace_volumes(plan, wall_material):
    return [
        PrismVolume(
            plan["id"] + f"/terrace-guard-{index}",
            shapely.set_precision(current["terrace_guard"], 0.0),
            current["floor_z_m"],
            current["floor_z_m"] + PARAPET_HEIGHT_M,
            wall_material,
        )
        for index, current in enumerate(plan["storey_plans"])
        if not current["terrace_guard"].is_empty
    ]


def exterior(plan, wall_material, inspect_layers=False):
    declared = layers(plan, wall_material)
    envelope = union_prisms(declared) if inspect_layers else None
    complete = union_prisms(
        [
            *declared,
            roof_volume(plan, wall_material),
            *terrace_volumes(plan, wall_material),
        ]
    )
    points = complete.mesh["vertices"]
    faces = complete.mesh["faces"]
    material = complete.mesh["materials"].copy()
    xyz = points[faces]
    normal = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    upward = normal[:, 2] > 0.5 * np.linalg.norm(normal, axis=1)
    material[(complete.source_indices < len(declared)) & upward] = 9
    mesh = dict(vertices=points, faces=faces, materials=material)
    mesh = corner_buffers(mesh, wall_material)
    return mesh, envelope, complete, "prism-union"


def near(plan, wall_material):
    shell = construct(plan)
    mesh, collision = assembly(plan, shell, frames=True)
    guard = roof_guard(plan, wall_material)
    offset = len(mesh["vertices"])
    mesh = dict(
        vertices=np.r_[mesh["vertices"], guard.mesh["vertices"]],
        faces=np.r_[mesh["faces"], guard.mesh["faces"] + offset],
        materials=np.r_[mesh["materials"], guard.mesh["materials"]],
    )
    collision = collision + native(guard.mesh, guard.vertex_roots)
    proof = access(plan, collision)
    columns = NativeColumns(collision, plan["base_z_m"] - 1, plan["source_top_m"] + 1)
    terraces = []
    for record in plan["terrace_access"]:
        current = level(plan, record["storey"])
        poses = landing_poses(np.asarray(record["path"]), current["floor_z_m"])
        terraces.append(
            dict(
                storey=record["storey"],
                component=record["component"],
                poses=len(poses),
                headroom_m=inspect_poses(columns, poses),
            )
        )
    raw = attach(detailed_mesh(plan), expand(slots(plan)))
    circulation(plan, raw)
    result = corner_buffers(mesh, wall_material)
    return result, proof, terraces, lights(plan, "mixed")
