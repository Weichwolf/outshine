"""Two real source continuations under one shared C1 projected height field."""

import numpy as np
import shapely
from adaptive_profile_surface import adaptive_mesh
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler, vertical_clearance
from outshine_ref.transport import (
    mesh_projected_profile,
    plan_vertical_transition,
    sample_vertical_transition,
)
from shared_bridge_geometry import corridor
from surface_height_checks import normal_gradients


def oriented_path(case):
    bridge, ground = case["bridge"], case["ground"]
    a, b = np.asarray(bridge["xy"], float), np.asarray(ground["xy"], float)
    if case["bridge_join_endpoint"] == 0:
        a = a[::-1]
    if case["ground_join_endpoint"] == 1:
        b = b[::-1]
    if np.linalg.norm(a[-1] - b[0]) > 1e-6:
        raise ValueError("original approach endpoint is not a common contact")
    props = bridge["properties"], ground["properties"]
    levels = props[0].get("layer", 1), props[1].get("layer", 0)
    if (
        props[0].get("brunnel") != "bridge"
        or props[1].get("brunnel") in ("bridge", "tunnel")
        or any(p.get("subclass") != "subway" or p.get("indoor") for p in props)
        or any(type(v) is not int for v in levels)
        or levels != (1, 0)
    ):
        raise ValueError("compatible explicit bridge/ground subway sources required")
    return np.vstack((a, b[1:])), ground["end_heights_m"][
        1 - case["ground_join_endpoint"]
    ]


def height_field(approaches, root, lower, method):
    if not 1 <= len(approaches) <= 16:
        raise ValueError("bounded nonempty source continuations required")
    paths, ends = zip(*(oriented_path(c) for c in approaches))
    starts = np.array([p[0] for p in paths])
    finish = np.array([p[-1] for p in paths])
    origin = starts.mean(axis=0)
    axis = finish.mean(axis=0) - origin
    distance = np.linalg.norm(axis)
    if not np.isfinite(distance) or distance < 1e-6:
        raise ValueError("separated finite projected contacts required")
    axis /= distance
    if not np.isfinite(axis).all() or any(
        np.any(np.diff(p @ axis) < -1e-6) for p in paths
    ):
        raise ValueError("one monotonic projected approach direction required")
    roots = shapely.coverage_union_all(
        shapely.polygons(root.vertices[root.triangles, :2])
    )
    extra = [corridor(s) for c in approaches for s in (c["bridge"], c["ground"])]
    domain = shapely.union_all([roots, *extra])
    low_domain = shapely.coverage_union_all(
        shapely.polygons(lower.vertices[lower.triangles, :2])
    )
    overlap = low_domain.intersection(domain)
    protected = shapely.get_coordinates(shapely.union_all([roots, overlap]))
    first = float(((protected - origin) @ axis).max()) + 2.0
    last = float(((finish - origin) @ axis).min()) - 4.0
    target = float(np.mean(ends))
    if max(abs(np.array(ends) - target)) > 0.1:
        raise ValueError("shared far-port height correction exceeds 0.1 m")
    root_height = float(root.vertices[0, 2])
    if abs(root.vertices[:, 2] - root_height).max() > 1e-7:
        raise ValueError("current joined deck must be one explicit height")
    if last <= first:
        raise ValueError("no approach space outside protected known surfaces")
    curve, report = plan_vertical_transition(
        root_height, target, last - first, grade=0.024, method=method
    )
    if curve is None:
        return None, report
    table, sampling = sample_vertical_transition(curve, height_error_m=0.01)
    if table is None:
        return None, sampling
    limits = (shapely.get_coordinates(domain) - origin) @ axis
    table[:, 0] += first
    table = np.vstack(
        ([limits.min() - 1, root_height], table, [limits.max() + 1, target])
    )
    report.update(
        origin_xy=origin.tolist(),
        direction_xy=axis.tolist(),
        transition_station_m=[first, last],
        far_height_m=target,
        individual_far_priors_m=list(ends),
        sampling=sampling,
        maximum_far_port_height_change_m=float(max(abs(np.array(ends) - target))),
        protected_root_and_known_lower_footprints=True,
        curve_breaks_m=curve.breaks.tolist(),
        curve_coefficients=curve.coefficients.tolist(),
        sampled_height_table=table.tolist(),
        analytic_scope="C1 height field z=h(dot(XY-origin,unit_axis)), flat outside its interval. Horizontal source paths are not made C1.",
    )
    return (domain, origin, axis, table, curve), report


def build(approaches, root, lower, *, method="parabolic", geometry_method="slices"):
    report = {
        "valid": False,
        "reason": "",
        "method": method,
        "geometry_method": geometry_method,
    }
    try:
        field, profile = height_field(approaches, root, lower, method)
        report["profile"] = profile
        if field is None:
            return None, dict(report, reason=profile["reason"])
        domain, origin, axis, _table, curve = field
        if geometry_method == "slices":
            mesh, geometry = mesh_projected_profile(
                domain,
                curve,
                origin,
                axis,
                start_station_m=profile["transition_station_m"][0],
            )
            if mesh is None:
                return None, dict(report, reason=geometry["reason"], geometry=geometry)
        elif geometry_method == "midpoints":
            mesh, geometry = adaptive_mesh(
                domain, origin, axis, curve, profile["transition_station_m"][0]
            )
        else:
            raise ValueError("known surface construction method required")
        gradients = normal_gradients(
            mesh.vertices[:, :2], mesh.triangles, mesh.vertices[:, 2]
        )
        maximum = float(np.linalg.norm(gradients, axis=1).max())
        report.update(geometry=geometry, maximum_gradient_norm=maximum)
        if maximum > 0.025 * (1 + 1e-7):
            return None, dict(report, reason="whole_face_approach_grade")
        same = SurfaceSampler(mesh).sample(root.vertices[:, :2])
        if (
            not np.isfinite(same.points_enu_m).all()
            or abs(same.points_enu_m[:, 2] - root.vertices[:, 2]).max() > 1e-7
        ):
            return None, dict(report, reason="existing_deck_contact_changed")
        gap = vertical_clearance(
            lower, Mesh(mesh.vertices - [0, 0, 0.35], mesh.triangles)
        )
        if not gap["valid"] or gap["minimum_clearance_m"] < 4.5 - 1e-7:
            return None, dict(
                report, reason=gap["reason"] or "known_full_surface_clearance"
            )
        ports = []
        for case in approaches:
            path, target = oriented_path(case)
            contact = SurfaceSampler(mesh).sample(path[[-1]])
            if not np.isfinite(contact.points_enu_m).all():
                raise ValueError("far source endpoint lost its chosen surface")
            ports.append(
                {
                    "source_id": case["ground"]["source_id"],
                    "point_enu_m": contact.points_enu_m[0].tolist(),
                    "original_DEM_prior_m": target,
                    "outside_connection": "tunnel_or_next_source_not_planned",
                }
            )
        report.update(
            valid=True,
            reason="",
            clearance=gap,
            far_ports=ports,
            maximum_existing_deck_contact_error_m=float(
                abs(same.points_enu_m[:, 2] - root.vertices[:, 2]).max()
            ),
            scope="One joined bridge plus selected real subway approaches. Known outdoor lower surfaces only; unresolved tunnels/indoor/elevated paths and other tile borders remain uncertified. Vertical C1 field, finite C0 mesh, no horizontal rail curves, terrain deformation, collision/driving or native admission.",
        )
        return mesh, report
    except ValueError as error:
        return None, dict(report, reason="invalid_approach_recipe", detail=str(error))
