import unittest
from itertools import product
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.buildings.window_bays import WindowBay, bay_flat, bay_skin


def surface_distance(points, product):
    triangles = product["vertices"][product["faces"]]
    u = triangles[:, 1] - triangles[:, 0]
    normal = np.cross(u, triangles[:, 2] - triangles[:, 0])
    normal /= np.linalg.norm(normal, axis=1, keepdims=True)
    u /= np.linalg.norm(u, axis=1, keepdims=True)
    v = np.cross(normal, u)
    local = triangles - triangles[:, :1]
    coords = np.stack(
        (np.einsum("tvi,ti->tv", local, u), np.einsum("tvi,ti->tv", local, v)), axis=2
    )
    polygons = shapely.polygons(coords)
    relative = points[:, None] - triangles[None, :, 0]
    xy = np.stack(
        (np.einsum("pti,ti->pt", relative, u), np.einsum("pti,ti->pt", relative, v)),
        axis=2,
    )
    planar = shapely.distance(shapely.points(xy), polygons[None])
    height = np.einsum("pti,ti->pt", relative, normal)
    return np.sqrt(planar**2 + height**2).min(axis=1)


def surface_samples(mesh):
    weights = np.array(
        [(i / 4, j / 4, 1 - (i + j) / 4) for i in range(5) for j in range(5 - i)]
    )
    return np.einsum("ij,tjk->tik", weights, mesh["vertices"][mesh["faces"]]).reshape(
        -1, 3
    )


class FlatWindowBays(unittest.TestCase):
    def test_exposed_surfaces_bound_both_directions_and_keep_material_normals(self):
        from test_window_bays import retained_surface_samples

        for fraction, depth, width in product(
            (0.65, 1.0), (0.65, 0.9), (1.8, 2.8, 3.2)
        ):
            plan = WindowBay(width, 2.9, depth_m=depth, front_fraction=fraction)
            old, flat = bay_skin(plan), bay_flat(plan, back_covered=True)
            self.assertEqual(len(flat["faces"]), 46)
            self.assertEqual(set(flat["roles"]), {0, 1, 2, 3})
            self.assertLessEqual(flat["vertices"][:, 1].max(), 0)
            bound = (plan.wall_m - plan.glass_m) / 2
            points = surface_samples(old)
            exposed = points[points[:, 1] <= 1e-12]
            self.assertLessEqual(surface_distance(exposed, flat).max(), bound + 1e-10)
            self.assertLessEqual(
                surface_distance(surface_samples(flat), old).max(), bound + 1e-10
            )
            self.assertLessEqual(retained_surface_samples(old, flat), bound + 1e-10)

    def test_back_cover_is_explicit_and_is_outside_the_exposed_error_domain(self):
        plan = WindowBay(2.8, 2.9)
        for cover in (False, 0, 1, "yes", None):
            with self.assertRaisesRegex(ValueError, "covered"):
                bay_flat(plan, back_covered=cover)
        old, flat = bay_skin(plan), bay_flat(plan, back_covered=True)
        rear = old["vertices"][old["vertices"][:, 1] == plan.back_m]
        self.assertGreater(
            surface_distance(rear, flat).max(), (plan.wall_m - plan.glass_m) / 2
        )

    def test_capacity_precedes_geometry_and_render_products_never_build_hidden_parts(
        self,
    ):
        plan = WindowBay(2.8, 2.9)
        with patch(
            "outshine_ref.buildings.window_bays._outline",
            side_effect=AssertionError("expanded"),
        ):
            for capacity in (45, True, 46.0):
                with self.assertRaisesRegex(ValueError, "capacity"):
                    bay_flat(plan, back_covered=True, max_triangles=capacity)
        with patch(
            "outshine_ref.buildings.window_bays.sections",
            side_effect=AssertionError("hidden parts"),
        ):
            self.assertEqual(len(bay_flat(plan, back_covered=True)["faces"]), 46)
            self.assertEqual(len(bay_skin(plan)["faces"]), 86)

    def test_render_buffers_are_owned_and_repeated_outputs_exact(self):
        plan = WindowBay(2.8, 2.9)
        a, b = bay_flat(plan, back_covered=True), bay_flat(plan, back_covered=True)
        for key in a:
            self.assertFalse(np.shares_memory(a[key], b[key]))
            np.testing.assert_array_equal(a[key], b[key])


if __name__ == "__main__":
    unittest.main()
