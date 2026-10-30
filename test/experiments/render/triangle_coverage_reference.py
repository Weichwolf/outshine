"""Pixel box integrals of a finite two-sided opaque triangle silhouette."""

import numpy as np
import shapely
from field_scale_projection import project
from urban_parent_areas import direct_areas


def front_source(source, eye=None, *, toward_eye=None):
    """Existing geometric face selection, with an explicit orthographic direction."""
    if (eye is None) == (toward_eye is None):
        raise ValueError(
            "Exactly one perspective eye or orthographic direction required"
        )
    parameter = np.asarray(eye if eye is not None else toward_eye)
    if (
        parameter.shape != (3,)
        or not np.isfinite(parameter).all()
        or (eye is None and not np.any(parameter != 0))
    ):
        raise ValueError("Finite three-dimensional eye/direction required")
    triangles = source["positions"].astype(float)[source["indices"]]
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    toward = (
        eye - triangles[:, 0]
        if eye is not None
        else np.broadcast_to(toward_eye, normals.shape)
    )
    front = np.einsum("ij,ij->i", normals, toward) > 0
    return {
        "positions": source["positions"],
        "indices": source["indices"][front],
    }, triangles.nbytes


def integrate(source, view, box):
    """Decline unsupported input; GEOS Double areas are not certified bounds."""
    positions, indices = np.asarray(source["positions"]), np.asarray(source["indices"])
    box = np.asarray(box)
    if (
        positions.ndim != 2
        or positions.shape[1:] != (3,)
        or not 0 < len(positions) <= 196608
        or indices.ndim != 2
        or indices.shape[1:] != (3,)
        or indices.dtype.kind not in "iu"
        or not 0 < len(indices) <= 65536
        or indices.min() < 0
        or indices.max() >= len(positions)
        or box.shape != (2, 2)
        or box.dtype.kind not in "iu"
        or np.any(box[0] < 0)
        or np.any(box[1] > [view[3], view[4]])
        or np.any(box[1] <= box[0])
        or int(box[1, 0] - box[0, 0]) * int(box[1, 1] - box[0, 1]) > 65536
    ):
        raise ValueError(
            "bounded indexed triangles and positive integer pixel ROI required"
        )
    xy = project(positions, view) - box[0]
    coordinates = xy[indices]
    polygons = shapely.polygons(coordinates)
    positive = shapely.area(polygons) > 0
    width, height = map(int, box[1] - box[0])
    silhouette = shapely.intersection(
        shapely.union_all(polygons[positive]), shapely.box(0, 0, width, height)
    )
    if not shapely.is_valid(silhouette):
        raise ValueError("projected union is not a valid finite silhouette")
    areas, work = direct_areas(silhouette, np.arange(width + 1), np.arange(height + 1))
    if not np.isfinite(areas).all() or np.any(areas < 0) or np.any(areas > 1):
        raise ValueError("polygon integration returned invalid pixel coverage")
    work.update(
        input_triangles=len(indices),
        positive_projected_triangles=int(positive.sum()),
        projected_vertex_bytes=xy.nbytes,
        gathered_triangle_coordinate_bytes=coordinates.nbytes,
        output_coverage_bytes=areas.nbytes,
        silhouette_coordinates=int(shapely.get_num_coordinates(silhouette)),
        area_sum_residual_px=float(areas.sum() - silhouette.area),
    )
    return areas, silhouette, work


def four_samples(silhouette, shape):
    """Fixed quarter-pixel points for comparison, never a filtering bound."""
    height, width = shape
    if not 0 < height * width <= 65536:
        raise ValueError("bounded positive pixel shape required")
    yy, xx = np.mgrid[:height, :width]
    centres = np.stack((xx, yy), axis=-1)[:, :, None, :]
    offsets = np.array([[0.25, 0.25], [0.75, 0.25], [0.25, 0.75], [0.75, 0.75]])
    points = shapely.points((centres + offsets).reshape(-1, 2))
    return shapely.covers(silhouette, points).reshape(height, width, 4).mean(axis=-1)
