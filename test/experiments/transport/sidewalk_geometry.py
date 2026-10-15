"""Whole affine edge contacts to the actual adjoining curb upper caps."""

import numpy as np
import shapely
from curb_geometry import audit as audit_volume
from outshine_ref.geometry import SurfaceSampler
from retained_surface import surface_domain


def joint_error(left, right, line):
    coordinates = []
    for mesh in [left, right]:
        faces = shapely.polygons(mesh.vertices[mesh.triangles, :2])
        pieces = shapely.intersection(faces, line)
        coordinates.append(shapely.get_coordinates(pieces))
    xy = np.unique(np.vstack(coordinates), axis=0)
    if not 1 < len(xy) <= 65536:
        raise ValueError("sidewalk joint coordinate budget or absence")
    values = [SurfaceSampler(m).sample(xy) for m in [left, right]]
    if any(any(n for n in v.reasons) for v in values):
        raise ValueError("whole sidewalk/curb joint is not on both actual caps")
    error = float(
        np.max(abs(values[0].points_enu_m[:, 2] - values[1].points_enu_m[:, 2]))
    )
    if error > 1e-7:
        raise ValueError("sidewalk/curb whole affine joint height mismatch")
    return error, len(xy)


def audit(product, row, curbs, shapes, context):
    proof = audit_volume(product, row["shape"], *context[:4], outside_limit=context[4])
    boundary = surface_domain(product.upper_contact).boundary
    joint = shapely.from_wkb(bytes.fromhex(row["joint_wkb_hex"]))
    actual_lines = []
    errors = []
    count = 0
    for index in row["curb_products"]:
        line = boundary.intersection(shapes[index].boundary)
        if line.length <= 1e-6:
            raise ValueError("actual sidewalk lost its declared curb joint")
        error, queries = joint_error(
            product.upper_contact, curbs[index].upper_contact, line
        )
        errors.append(error)
        count += queries
        actual_lines.append(line)
    actual_joint = shapely.union_all(actual_lines)
    if (
        actual_joint.length <= 1e-6
        or actual_joint.symmetric_difference(joint).length > 1e-7
    ):
        raise ValueError("actual full sidewalk/curb joint coverage differs")
    return dict(
        proof,
        joint_length_m=actual_joint.length,
        maximum_joint_height_error_m=max(errors),
        affine_joint_endpoint_queries=count,
    )
