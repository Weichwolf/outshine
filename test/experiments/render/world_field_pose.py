"""World/ECEF pose mapped to the existing local-field geometric bound.

Compose enclosed clip rows after relative Double translation. No nominal rigid
inverse, local camera normalization, GPU arithmetic or World admission is assumed.
"""

import math

from field_pose_geometry import operands, projection_matrix_bound
from field_view_projection import dot, point, subtract


def rigid_frame(frame):
    rows = tuple(tuple(float(v) for v in row) for row in frame)
    if (
        len(rows) != 4
        or any(len(row) != 4 for row in rows)
        or not all(math.isfinite(v) for row in rows for v in row)
        or rows[3] != (0.0, 0.0, 0.0, 1.0)
    ):
        raise ValueError("Finite affine WorldFromField required")
    columns = tuple(tuple(rows[i][j] for i in range(3)) for j in range(3))
    tolerance = 1e-10  # Same schema tolerance as BuildingCaptureFrame::Valid.
    if any(
        abs(sum(x * y for x, y in zip(a, b, strict=True)) - (i == j)) > tolerance
        for i, a in enumerate(columns)
        for j, b in enumerate(columns)
    ):
        raise ValueError("Native rigid source frame required; scale/shear unsupported")
    a, b, c = columns
    determinant = (
        a[0] * (b[1] * c[2] - b[2] * c[1])
        - a[1] * (b[0] * c[2] - b[2] * c[0])
        + a[2] * (b[0] * c[1] - b[1] * c[0])
    )
    if determinant < 1 - tolerance:
        raise ValueError("Proper native source orientation required")
    return rows


def clip_from_field(world_from_field, eye_world, basis_world, projection, raster):
    """Enclose P * [A*R, A*(T-eye); 0,1] using supplied encoded values.

    Input WorldFromField is four ROWS; decode native column-major storage explicitly.
    R/T come from the bound metadata. Camera axes are covectors: no inverse or
    renormalization changes their meaning. Rigid validation is schema validation,
    not a tolerance for frame identity or a declaration of exact orthogonality.
    """
    rows = rigid_frame(world_from_field)
    eye, basis, optical, raster = operands(eye_world, basis_world, projection, raster)
    relative = tuple(subtract(point(rows[i][3]), point(eye[i])) for i in range(3))
    rotation = tuple(tuple(point(rows[i][j]) for i in range(3)) for j in range(3))
    camera = tuple(
        (
            *[dot(tuple(map(point, axis)), column) for column in rotation],
            dot(tuple(map(point, axis)), relative),
        )
        for axis in basis
    ) + ((point(0.0), point(0.0), point(0.0), point(1.0)),)
    clip = tuple(
        tuple(dot(row, tuple(camera[k][j] for k in range(4))) for j in range(4))
        for row in optical
    )
    return clip, raster


def projection_bound(
    plane,
    world_from_field,
    eye_world,
    basis_world,
    projection,
    raster,
    *,
    normal=None,
    source_hull=None,
):
    if world_from_field is None:
        return {
            "screen_l1_upper_px": None,
            "depth_inside": False,
            "reason": "world_from_field_unavailable",
            "world_admission": False,
        }
    try:
        clip, raster = clip_from_field(
            world_from_field, eye_world, basis_world, projection, raster
        )
        result = projection_matrix_bound(
            plane, clip, raster, normal=normal, source_hull=source_hull
        )
    except (TypeError, ValueError, OverflowError):
        return {
            "screen_l1_upper_px": None,
            "depth_inside": False,
            "reason": "world_pose_or_frame_unqualified",
            "world_admission": False,
        }
    return {
        **result,
        "world_admission": False,
        "frame_model": "relative_double_interval_clip_from_field",
        "clip_from_field_intervals": clip,
    }
