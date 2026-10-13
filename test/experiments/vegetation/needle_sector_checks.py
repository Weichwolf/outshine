"""Independent section partitions, asymmetric swept volume and edge checks."""

import numpy as np
from scipy.integrate import quad
from shapely.geometry import Polygon
from shapely.ops import unary_union


def swept_volume(shape):
    alpha = 2 * np.pi / shape.sector_count
    area = alpha * shape.aspect / 2
    centroid = shape.aspect * 4 * np.sin(alpha / 2) / (3 * alpha)

    def integrand(t):
        q = t / shape.taper_start
        profile = (
            0.55 + 0.45 * q**2 * (3 - 2 * q)
            if t <= shape.taper_start
            else 1 - ((t - shape.taper_start) / (1 - shape.taper_start)) ** 2
        )
        radius = shape.radius * profile
        axial = shape.length - shape.bend * radius * centroid * np.cos(shape.twist * t)
        return area * radius**2 * axial

    return quad(integrand, 0, 1, points=[shape.taper_start], epsabs=1e-18)[0]


def sector_edges(shape):
    points = 0
    for angle, left, right in ((0, 2, 0), (np.pi / 2, 0, 1), (3 * np.pi / 2, 1, 2)):
        t = np.linspace(0, 0.99, 31)
        p, n, _ = shape.evaluate(t, angle, left)
        q, m, _ = shape.evaluate(t, angle, right)
        assert np.allclose(p, q, atol=1e-18, rtol=0)
        if angle != 0 or shape.sector_count != 2:
            assert np.all(np.linalg.norm(n - m, axis=1) > 0.05)
        else:
            assert np.allclose(n, m, atol=1e-12, rtol=0)
        points += len(t)
    return points


def fascicle_partitions():
    from needle_recipes import needle_graph

    records = []
    for case in (0, 1):
        graph = needle_graph(case)
        shape = graph.by_id[3].shape
        poses = graph.instances(3, 1000)
        groups = {}
        for pose in poses:
            groups.setdefault(pose.path[:-1], []).append(pose)
        errors, overlaps = [], []
        for bundle in groups.values():
            first = bundle[0]
            radius = 0.55 * shape.radius * first.scale
            polygons = []
            for pose in bundle:
                assert np.array_equal(pose.origin, first.origin)
                assert pose.scale == first.scale
                angles = np.r_[0, np.linspace(np.pi / 2, 3 * np.pi / 2, 129)]
                local = shape.evaluate(np.zeros_like(angles), angles)[0]
                world = (local * pose.scale) @ pose.frame.T
                projected = (world @ first.frame)[:, [0, 2]] / radius
                polygon = Polygon(projected)
                assert polygon.is_valid and polygon.area > 0
                polygons.append(polygon)
            union = unary_union(polygons)
            target = np.pi * shape.aspect
            errors.append(abs(union.area / target - 1))
            overlap = sum(
                a.intersection(b).area
                for i, a in enumerate(polygons)
                for b in polygons[i + 1 :]
            )
            overlaps.append(overlap / target)
        assert max(errors) < 1.1e-4 and max(overlaps) < 1e-12
        records.append(
            {
                "case": case,
                "bundles": len(groups),
                "needles_per_bundle": shape.sector_count,
                "base_union_relative_error": max(errors),
                "base_overlap_fraction": max(overlaps),
            }
        )
    return records


def sector_checks():
    from needle_checks import check_mesh
    from needle_surface import Needle

    cases, maximum, points, corners = [], 0.0, 0, 0
    for count, bend, twist in ((2, 0.25, 0.5), (3, -0.8, 2), (5, 0.4, 0.65), (8, 0, 0)):
        shape = Needle(section="sector", sector_count=count, bend=bend, twist=twist)
        t = np.tile(np.linspace(0.01, 0.98, 89), 3)
        angle = np.repeat([0.37, np.pi, 5.7], 89)
        _, normal, density = shape.evaluate(t, angle)
        step = 1e-6
        dt = (
            shape.evaluate(t + step, angle)[0] - shape.evaluate(t - step, angle)[0]
        ) / (2 * step)
        da = (
            shape.evaluate(t, angle + step)[0] - shape.evaluate(t, angle - step)[0]
        ) / (2 * step)
        cross = np.cross(dt, da)
        lengths = np.linalg.norm(cross, axis=1)
        error = float(np.max(abs(normal - cross / lengths[:, None])))
        maximum = max(maximum, error)
        assert error < 1e-8 and np.allclose(density, lengths, rtol=1e-7, atol=1e-14)
        points += len(t)
        corners += sector_edges(shape)
        near, coarse = shape.mesh(), shape.mesh(16, 8)
        reference = swept_volume(shape)
        errors = [check_mesh(mesh) / reference - 1 for mesh in (near, coarse)]
        assert all(-0.15 < error < 0 for error in errors)
        assert all(
            any(np.array_equal(p, q) for q in near["vertices"])
            for p in coarse["vertices"]
        )
        scalar = shape.mesh(method="scalar")
        assert all(np.array_equal(near[k], scalar[k]) for k in near)
        cases.append(
            {
                "section": f"Sektor {count}",
                "near_triangles": len(near["faces"]),
                "coarse_triangles": len(coarse["faces"]),
                "common_vertices": len(coarse["vertices"]),
                "volume_relative_errors": errors,
                "asymmetric_volume_m3": reference,
            }
        )
    for count in (0, 1, 9, 2.0, True):
        try:
            Needle(section="sector", sector_count=count)
        except ValueError:
            continue
        raise AssertionError("invalid sector count accepted")
    return {
        "cases": cases,
        "differential_points": points,
        "normal_max_error": maximum,
        "edge_pairs": corners,
        "fascicles": fascicle_partitions(),
    }
