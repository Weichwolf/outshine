"""Compare clipping retained cells with rebuilding their identical traffic footprint."""

import time

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from retained_surface import surface_domain
from transport_flat_surfaces import shared_mesh


def union_traffic(old, new, method, *, max_faces=262144):
    if method not in ("cells", "domain"):
        raise ValueError("cell clipping or domain rebuilding required")
    began = time.monotonic()
    if type(max_faces) is not int or not 1 <= max_faces <= 262144:
        raise ValueError("bounded traffic union allocation required")
    if len(old.triangles) + len(new.triangles) > max_faces:
        raise ValueError("traffic union input face budget")
    old_domain, new_domain = surface_domain(old), surface_domain(new)
    domain = old_domain.union(new_domain)
    if method == "domain":
        # Internal flat module cuts remain in the 2D product. The height mesh
        # needs the same physical footprint, rather than every obsolete cut.
        parts = shapely.get_parts(domain)
    else:
        cells = shapely.polygons(new.vertices[new.triangles, :2])
        parts = np.r_[
            shapely.polygons(old.vertices[old.triangles, :2]),
            shapely.get_parts(shapely.difference(cells, old_domain)),
        ]
    parts = parts[(shapely.get_type_id(parts) == 3) & (shapely.area(parts) > 0)]
    if shapely.get_num_coordinates(parts).sum() > 1048576:
        raise ValueError("traffic union boundary coordinate budget")
    product, report = shared_mesh(domain, parts, np.zeros(len(parts), int), 1e-4)
    if len(product["vertices"]) > max_faces or len(product["indices"]) > max_faces:
        raise ValueError("traffic union output face/vertex budget")
    result = Mesh(
        np.c_[product["vertices"], np.zeros(len(product["vertices"]))],
        product["indices"],
    )
    error = surface_domain(result).symmetric_difference(domain).area
    if error > 1e-7:
        raise ValueError("retained traffic union lost physical footprint")
    return result, dict(
        report,
        method=method,
        input_faces=len(old.triangles) + len(new.triangles),
        vertices=len(result.vertices),
        faces=len(result.triangles),
        old_missing_m2=old_domain.difference(surface_domain(result)).area,
        footprint_error_m2=error,
        total_ms=(time.monotonic() - began) * 1000,
    )
