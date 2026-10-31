"""Remove a supplied bank footprint; retain original exterior terrain facet heights.

The result still needs interface lifting and matching body-edge subdivisions.
"""

import numpy as np
import shapely
from pool_body import positive_faces


def cut(vertices, faces, footprint, *, method="local", max_faces=32768):
    if len(faces) > 16384 or len(vertices) > 16384 or not footprint.is_valid:
        raise ValueError("bounded terrain and valid bank footprint required")
    original = shapely.polygons(vertices[faces, :2])
    if not shapely.coverage_is_valid(original):
        raise ValueError("conforming original terrain required")
    if method == "local":
        selected = np.sort(
            shapely.STRtree(original).query(footprint, predicate="intersects")
        )
    elif method == "global":
        selected = np.arange(len(faces))
    else:
        raise ValueError("known local or global reference method required")
    pieces, parents = shapely.get_parts(
        shapely.difference(original[selected], footprint), return_index=True
    )
    pieces = shapely.constrained_delaunay_triangles(pieces)
    triangles, owners = shapely.get_parts(pieces, return_index=True)
    untouched = np.setdiff1d(np.arange(len(faces)), selected)
    triangles = np.r_[triangles, original[untouched]]
    source_faces = np.r_[selected[parents[owners]], untouched]
    if len(triangles) > max_faces or not shapely.coverage_is_valid(triangles):
        raise ValueError("bounded conforming terrain cut required")
    expected = shapely.difference(shapely.coverage_union_all(original), footprint)
    actual = shapely.coverage_union_all(triangles)
    boundary_error = float(
        shapely.hausdorff_distance(actual.boundary, expected.boundary)
    )
    tolerance = (
        128 * np.finfo(float).eps * max(1.0, float(np.abs(vertices[:, :2]).max()))
    )
    # GEOS's topological equals can disagree with its empty overlay difference
    # after repeated noding. Retain that predicate result in the diagnostic; do
    # not replace the independent contact-edge audit with an area tolerance.
    if not actual.symmetric_difference(expected).is_empty or boundary_error > tolerance:
        raise ValueError("terrain cut does not preserve its full exterior domain")
    coordinates = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
    xy, first, inverse = np.unique(
        coordinates.reshape(-1, 2), axis=0, return_index=True, return_inverse=True
    )
    # Each child retains its particular original affine terrain facet. Preserve
    # existing original XYZ words before evaluating only newly constructed nodes.
    lookup = {tuple(p[:2]): p for p in vertices}
    xyz = np.c_[xy, np.full(len(xy), np.nan)]
    for i, point in enumerate(xy):
        if tuple(point) in lookup:
            xyz[i] = lookup[tuple(point)]
    chosen = np.flatnonzero(~np.isfinite(xyz[:, 2]))
    source = vertices[faces[source_faces[first[chosen] // 3]]]
    normal = np.cross(source[:, 1] - source[:, 0], source[:, 2] - source[:, 0])
    xyz[chosen, 2] = (
        source[:, 0, 2]
        - np.sum((xy[chosen] - source[:, 0, :2]) * normal[:, :2], axis=1) / normal[:, 2]
    )
    return (
        xyz,
        positive_faces(xy, inverse.reshape(-1, 3)),
        source_faces,
        {
            "input_faces": len(faces),
            "boolean_faces": len(selected),
            "untouched_faces": len(untouched),
            "output_faces": len(triangles),
            "new_nodes": sum(tuple(p) not in lookup for p in xy),
            "coordinate_gather_bytes": coordinates.nbytes,
            "full_exterior_predicate_equal": bool(actual.equals(expected)),
            "exterior_boundary_hausdorff_m": boundary_error,
        },
    )
