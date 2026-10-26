"""Bounded indexed 2D cuts before source-height identities, with explicit selections."""

import numpy as np
from stair_station_cuts import _frame, _split


def _inputs(xy, faces, masks, axes, max_vertices, max_faces):
    xy, faces, masks = (
        np.asarray(xy, dtype=np.float64),
        np.asarray(faces),
        np.asarray(masks),
    )
    if (
        xy.ndim != 2
        or xy.shape[1] != 2
        or not 0 < len(xy) <= max_vertices
        or not np.isfinite(xy).all()
        or faces.ndim != 2
        or faces.shape[1] != 3
        or not 0 < len(faces) <= max_faces
        or not np.issubdtype(faces.dtype, np.integer)
        or faces.min() < 0
        or faces.max() >= len(xy)
        or len(masks) != len(faces)
        or masks.ndim != 1
        or not np.issubdtype(masks.dtype, np.integer)
        or any(not 0 < int(mask) < 2**64 for mask in masks)
    ):
        raise ValueError("bounded finite indexed canonical source mesh required")
    delta = xy[faces[:, 1:]] - xy[faces[:, 0, None]]
    area = delta[:, 0, 0] * delta[:, 1, 1] - delta[:, 0, 1] * delta[:, 1, 0]
    if not np.isfinite(area).all() or np.any(area <= 0):
        raise ValueError("positive finite canonical input faces required")
    if len(axes) > 16 or sum(len(a["cuts_m"]) for a in axes) > 32:
        raise ValueError("canonical axis or cut budget exhausted")
    flights, selections = [], []
    for axis in axes:
        origin, tangent = np.asarray(axis["origin_xy_m"]), np.asarray(axis["tangent"])
        if (
            origin.shape != (2,)
            or tangent.shape != (2,)
            or not np.isfinite(np.r_[origin, tangent, axis["cuts_m"]]).all()
            or not np.any(tangent)
        ):
            raise ValueError("finite nonzero canonical cut frame required")
        selected = axis["initial_faces"]
        if selected is not None and any(
            not isinstance(i, (int, np.integer)) or not 0 <= i < len(faces)
            for i in selected
        ):
            raise ValueError("canonical selection must name original face indices")
        delta = xy - origin
        stations = delta[:, 0] * tangent[0] + delta[:, 1] * tangent[1]
        if not np.isfinite(stations).all():
            raise ValueError("finite canonical input stations required")
        flights.append(
            {"origin_xy_m": origin, "tangent": tangent, "stations_m": stations.tolist()}
        )
        selections.append(None if selected is None else set(selected))
    return xy, faces, flights, selections


def refine(
    xy,
    faces,
    source_masks,
    axes,
    *,
    max_vertices=4096,
    max_faces=8192,
    max_face_visits=262144,
    cancelled=None,
):
    xy, faces, flights, selections = _inputs(
        xy, faces, source_masks, axes, max_vertices, max_faces
    )
    vertices, triangles = [p.copy() for p in xy], faces.tolist()
    lineage, planar = list(range(len(faces))), list(range(len(xy)))
    cuts = {}
    for axis, (flight, selected) in enumerate(zip(flights, selections, strict=True)):
        for cut in axes[axis]["cuts_m"]:
            key = _frame(flight), float(cut)
            if key not in cuts:
                cuts[key] = axis, selected
            else:
                first, previous = cuts[key]
                cuts[key] = (
                    first,
                    (
                        None
                        if previous is None or selected is None
                        else previous | selected
                    ),
                )
    visits, intersections = 0, 0
    for (_frame_key, cut), (axis, selected) in cuts.items():
        if cancelled is not None and cancelled():
            raise ValueError("canonical station refinement cancelled")
        if selected == set():
            continue
        if visits + len(triangles) > max_face_visits:
            raise ValueError("canonical station face-visit budget exhausted")
        visits += len(triangles)
        selected_masks = (
            [int(p in selected) for p in lineage]
            if selected is not None
            else [1] * len(triangles)
        )
        triangles, _selected, lineage, count, _points = _split(
            vertices,
            flights,
            triangles,
            selected_masks,
            lineage,
            planar,
            axis,
            cut,
            max_vertices,
            max_faces,
            1 if selected is not None else None,
        )
        intersections += count
    if cancelled is not None and cancelled():
        raise ValueError("canonical station refinement cancelled")
    parents = np.asarray(lineage, dtype=np.uint32)
    return {
        "xy_m": np.asarray(vertices, dtype=np.float64),
        "triangles": np.asarray(triangles, dtype=np.uint32),
        "source_masks": np.asarray(source_masks, dtype=np.uint64)[parents],
        "parents": parents,
        "stations_m": np.asarray(
            [f["stations_m"] for f in flights], dtype=np.float64
        ).reshape(len(flights), len(vertices)),
        "axes": axes,
        "input_mesh": {
            "xy_m": xy,
            "triangles": faces,
            "source_masks": np.asarray(source_masks),
        },
        "work": {
            "input_vertices": len(xy),
            "input_faces": len(faces),
            "output_vertices": len(vertices),
            "output_faces": len(triangles),
            "face_visits": visits,
            "edge_intersections": intersections,
            "unique_station_cuts": len(cuts),
            "declared_station_cuts": sum(len(a["cuts_m"]) for a in axes),
            "xy_vertex_copy_count": len(xy),
            "xy_vertex_copy_bytes": xy.nbytes,
            "output_xy_materialized_bytes": len(vertices) * 16,
            "output_face_parent_mask_materialized_bytes": len(triangles) * 24,
            "output_station_materialized_bytes": len(flights) * len(vertices) * 8,
        },
    }
