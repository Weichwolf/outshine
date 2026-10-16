import unittest
from itertools import product
from unittest.mock import patch

import numpy as np
from outshine_ref.buildings.window_bays import (
    WindowBay,
    bay_skin,
    bay_surface,
    sections,
    volumes,
)


def native(mesh):
    import manifold3d as manifold
    from outshine_ref.buildings.modelling.mesh_fans import connected_fans

    if "parts" in mesh:
        solids = [
            native(
                {
                    "vertices": mesh["vertices"],
                    "faces": mesh["faces"][mesh["parts"] == part],
                }
            )
            for part in np.unique(mesh["parts"])
        ]
        return manifold.Manifold.batch_boolean(solids, manifold.OpType.Add)

    p, f, duplicates, targets = connected_fans(
        mesh | {"uv": np.zeros((len(mesh["vertices"]), 2))},
        np.arange(len(mesh["faces"])),
    )
    result = manifold.Manifold(
        manifold.Mesh64(
            np.ascontiguousarray(p[:, :3]),
            f,
            merge_from_vert=duplicates,
            merge_to_vert=targets,
        )
    )
    if result.status() != manifold.Error.NoError:
        raise AssertionError(result.status())
    return result


def retained_surface_samples(full, skin):
    triangles = full["vertices"][full["faces"]]
    a, u, v = (
        triangles[:, 0],
        triangles[:, 1] - triangles[:, 0],
        triangles[:, 2] - triangles[:, 0],
    )
    normals = np.cross(u, v)
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    skin_triangles = skin["vertices"][skin["faces"]]
    ns = np.cross(
        skin_triangles[:, 1] - skin_triangles[:, 0],
        skin_triangles[:, 2] - skin_triangles[:, 0],
    )
    ns /= np.linalg.norm(ns, axis=1, keepdims=True)
    weights = np.array(
        [(i / 4, j / 4, 1 - (i + j) / 4) for i in range(5) for j in range(5 - i)]
    )
    points = np.einsum("ij,tjk->tik", weights, skin_triangles)
    delta = points[:, :, None] - a[None, None]
    distance = np.einsum("tpfi,fi->tpf", delta, normals)
    pu, pv = np.einsum("tpfi,fi->tpf", delta, u), np.einsum("tpfi,fi->tpf", delta, v)
    uu, vv, uv = (u * u).sum(axis=1), (v * v).sum(axis=1), (u * v).sum(axis=1)
    denominator = uu * vv - uv * uv
    b, c = (vv * pu - uv * pv) / denominator, (uu * pv - uv * pu) / denominator
    valid = (b >= -1e-8) & (c >= -1e-8) & (b + c <= 1 + 1e-8)
    valid &= (ns @ normals.T > 0.999999)[:, None]
    valid &= (skin["roles"][:, None] == full["roles"][None])[:, None]
    return np.where(valid, abs(distance), np.inf).min(axis=2).max()


class WindowBays(unittest.TestCase):
    def test_both_shapes_preserve_closed_volumes_and_open_back(self):
        from outshine_ref.buildings.modelling.prism_union import primitive_mesh

        for fraction in (0.65, 1.0):
            for width in (1.8, 2.8, 3.2):
                plan = WindowBay(width, 2.9, front_fraction=fraction)
                shape = sections(plan)
                area = (
                    width * (plan.back_m + plan.depth_m)
                    - width * (1 - fraction) * plan.depth_m / 2
                )
                self.assertAlmostEqual(shape["outer"].area, area, places=12)
                expected = 0.0
                for source in volumes(plan):
                    vertices, faces = primitive_mesh(source)
                    solid = native({"vertices": vertices, "faces": faces})
                    volume = source.polygon.area * (source.high_m - source.low_m)
                    self.assertAlmostEqual(solid.volume(), volume, places=10)
                    expected += volume
                full = bay_surface(plan, method="parts")
                self.assertAlmostEqual(native(full).volume(), expected, places=10)
                self.assertEqual(set(full["roles"]), {0, 1, 2, 3})
                import shapely

                rear = shapely.Point(0, plan.back_m - 0.001)
                self.assertTrue(shape["inside"].covers(rear))
                self.assertFalse(shape["wall"].covers(rear))
                half = width / 2 - plan.wall_m
                inner = (
                    shape["outer"]
                    .buffer(-plan.wall_m, join_style=2)
                    .union(
                        shapely.box(-half, plan.back_m - plan.wall_m, half, plan.back_m)
                    )
                )
                self.assertLess(inner.symmetric_difference(shape["inside"]).area, 1e-10)
                actual = native(full).scale((1000.0,) * 3)
                glazing = shapely.union_all([shape["posts"], *shape["panes"]])
                for low, high, contour in (
                    (0, plan.floor_m, shape["outer"]),
                    (plan.floor_m, plan.sill_m, shape["wall"]),
                    (plan.sill_m, plan.head_m, glazing),
                    (plan.head_m, plan.height_m - plan.ceiling_m, shape["wall"]),
                    (plan.height_m - plan.ceiling_m, plan.height_m, shape["outer"]),
                ):
                    rings = [
                        shapely.Polygon(np.asarray(ring) / 1000)
                        for ring in actual.slice((low + high) * 500).to_polygons()
                    ]
                    sliced = shapely.Polygon()
                    for ring in sorted(rings, key=lambda p: -p.area):
                        sliced = sliced.symmetric_difference(ring)
                    self.assertLess(sliced.symmetric_difference(contour).area, 1e-7)

    def test_render_skin_samples_match_actual_physical_exterior_roles_and_normals(self):
        for fraction, depth, width in product(
            (0.65, 1.0), (0.65, 0.9), (1.8, 2.8, 3.2)
        ):
            plan = WindowBay(width, 2.9, front_fraction=fraction, depth_m=depth)
            skin, full = bay_skin(plan), bay_surface(plan, method="parts")
            self.assertEqual(len(skin["faces"]), 86)
            self.assertLess(retained_surface_samples(full, skin), 1e-9)
            self.assertEqual(set(skin["roles"]), {0, 1, 2, 3})

    def test_capacity_before_expansion_and_caller_owned_buffers(self):
        plan = WindowBay(2.8, 2.9)
        with (
            patch(
                "outshine_ref.buildings.window_bays.union_prisms",
                side_effect=AssertionError("expanded"),
            ),
            patch(
                "outshine_ref.buildings.window_bays.primitive_mesh",
                side_effect=AssertionError("expanded"),
            ),
        ):
            for method in ("union", "parts"):
                with self.assertRaisesRegex(ValueError, "capacity"):
                    bay_surface(plan, method=method, max_triangles=1)
        with self.assertRaisesRegex(ValueError, "capacity"):
            bay_skin(plan, max_triangles=85)
        for method in ("union", "parts"):
            a, b = bay_surface(plan, method=method), bay_surface(plan, method=method)
            for key in a:
                self.assertFalse(np.shares_memory(a[key], b[key]))
                np.testing.assert_array_equal(a[key], b[key])

    def test_invalid_or_unresolved_dimensions_fail(self):
        for values in (
            {"width_m": True},
            {"height_m": 2},
            {"front_fraction": 0.1},
            {"front_fraction": 1.01},
            {"wall_m": 0.5},
            {"head_m": 0.5},
            {"width_m": float("inf")},
            {"width_m": 1e100},
        ):
            with self.assertRaises(ValueError):
                WindowBay(**({"width_m": 2.8, "height_m": 2.9} | values))
        with self.assertRaisesRegex(ValueError, "too short"):
            sections(WindowBay(2.8, 2.9, depth_m=0.45, front_fraction=1))

    def test_union_mesh_roundtrip_rejects_known_chamfered_counterexamples(self):
        for width in (1.8, 3.2):
            with self.assertRaisesRegex(ValueError, "roundtrip"):
                bay_surface(WindowBay(width, 2.9), method="union")
        for fraction in (0.65, 1.0):
            plan = WindowBay(2.8, 2.9, front_fraction=fraction)
            full = bay_surface(plan, method="union")
            self.assertAlmostEqual(
                native(full).volume(), native(bay_surface(plan)).volume(), places=10
            )


if __name__ == "__main__":
    unittest.main()
