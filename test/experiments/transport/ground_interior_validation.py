"""Independent unfitted controls and original-frame bridge composition."""

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import vertical_clearance


def validation_inputs(context, base):
    mesh = base["mesh"]
    faces = mesh.triangles[len(context.road.triangles) :]
    triangles = mesh.vertices[faces]
    area = (
        np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0])[
            :, 2
        ]
        / 2
    )
    bary = (
        np.array(
            [[i, j, 8 - i - j] for i in range(9) for j in range(9 - i)], dtype=float
        )
        / 8
    )
    xy = np.einsum("sj,fji->fsi", bary, triangles[:, :, :2])
    n = len(base["coarse_mesh"].vertices)
    weights = np.zeros(len(mesh.vertices))
    np.add.at(weights, faces.ravel(), np.repeat(area / 3, 3))
    return {
        "faces": faces,
        "area": area,
        "bary": bary,
        "dense_xy": xy,
        "dense_dem_z": context.sample_z(xy.reshape(-1, 2)).reshape(xy.shape[:2]),
        "centroid_dem_z": context.sample_z(triangles[:, :, :2].mean(axis=1)),
        "free_dem_z": context.sample_z(mesh.vertices[n:, :2]),
        "free_weights": weights[n:],
        "fixed_vertices": n,
    }


def validate(mesh, inputs):
    values = mesh.vertices[inputs["faces"], 2]
    centroid = values.mean(axis=1) - inputs["centroid_dem_z"]
    dense = np.einsum("sj,fj->fs", inputs["bary"], values) - inputs["dense_dem_z"]
    free = mesh.vertices[inputs["fixed_vertices"] :, 2] - inputs["free_dem_z"]
    return {
        "centroid_max_abs_error_m": float(abs(centroid).max()),
        "centroid_area_weighted_rms_m": float(
            np.sqrt(np.average(centroid**2, weights=inputs["area"]))
        ),
        "dense_max_abs_error_m": float(abs(dense).max()),
        "dense_area_weighted_rms_m": float(
            np.sqrt(np.average(np.mean(dense**2, axis=1), weights=inputs["area"]))
        ),
        "free_area_weighted_rms_m": float(
            np.sqrt(np.average(free**2, weights=inputs["free_weights"]))
        ),
    }


def bridge_composition():
    from bridge_end_body import build_end_body
    from bridge_end_surface_inputs import actual_end_inputs
    from ground_contact import ContactLimits
    from ground_contact_checks import qualify_contact
    from ground_contact_inputs import ContactContext, load_context
    from ground_interior import relax_contact
    from ground_rim_contact import build_rim_contact

    _, index, prepared, ground, _ = actual_end_inputs()
    template = load_context()
    context = ContactContext(
        ground["surface"],
        prepared[0],
        template.terrain,
        np.empty(0),
        ground,
        template.metadata,
        0,
    )
    context.road_dem_z = context.sample_z(context.road.vertices[:, :2])
    base = build_rim_contact(
        context.road,
        context.road_dem_z,
        context.sample_z,
        limits=ContactLimits(margins_m=(15.0,)),
    )
    result = relax_contact(context.road, base, context.sample_z)
    quality = qualify_contact(context.road, result)
    soil = Mesh(
        result["mesh"].vertices, result["mesh"].triangles[len(context.road.triangles) :]
    )
    rows = []
    arrays = {
        "bridge_soil_vertices": soil.vertices,
        "bridge_soil_faces": soil.triangles,
    }
    for terminal in ground["terminal_values"]:
        group = index.groups_by_node[terminal["source_terminal"]["source_node"]]
        if not any(index.index[1][member.edge[:2]].bridge for member in group.members):
            continue
        product, _ = build_end_body(prepared, ground, index, group.identifier)
        clearance = vertical_clearance(
            soil, product["surface"], seconds=5.0, include_maximum=True
        )
        if not clearance["valid"] or clearance["minimum_clearance_m"] < -1e-8:
            raise ValueError("terrain buries original bridge walking surface")
        rows.append({"group": group.identifier, "soil_to_top": clearance})
        key = "bridge_" + group.identifier.rsplit(":", 1)[-1]
        arrays[key + "_vertices"] = product["body"].vertices
        arrays[key + "_faces"] = product["body"].triangles
    if len(rows) != 2:
        raise ValueError("actual source-bound bridge composition missing")
    return {
        "quality": quality,
        "bridges": rows,
        "source_binding": ground["binding"],
    }, arrays
