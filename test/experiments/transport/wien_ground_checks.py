"""Independent whole-surface checks and analytic constrained-ground counterexamples."""

import unittest

import numpy as np
from bounded_ground_fit import fit
from ground_affine_bounds import joined, overlap_bounds, vertex_caps
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import vertical_clearance
from retained_surface import surface_domain
from surface_height_checks import normal_gradients
from transport_gallery_wien import replay


def check(arrays, heights, model, previous, old_note, domain):
    mesh = Mesh(np.c_[arrays["xy"], heights], arrays["faces"])
    road = Mesh(mesh.vertices, mesh.triangles[: int(arrays["road_count"])])
    gradient = normal_gradients(arrays["xy"], arrays["faces"], heights)
    grade = np.linalg.norm(np.einsum("fij,fj->fi", arrays["frames"], gradient), axis=1)
    changed = heights[arrays["old_vertex_map"]] - arrays["old_heights"]
    bridge = vertical_clearance(
        road, Mesh(previous[0].vertices - [0, 0, 0.35], previous[0].triangles)
    )
    soil_gap = vertical_clearance(
        mesh, Mesh(previous[0].vertices - [0, 0, 0.35], previous[0].triangles)
    )
    cover = vertical_clearance(
        model["roof"], Mesh(mesh.vertices - [0, 0, 0.25], mesh.triangles)
    )
    domain_error = surface_domain(mesh).symmetric_difference(domain).area
    if (
        grade.max() > 1 + 1e-7
        or changed.max() > 1e-7
        or not bridge["valid"]
        or bridge["minimum_clearance_m"] < 4.5 - 1e-7
        or not soil_gap["valid"]
        or soil_gap["minimum_clearance_m"] < 0.02 - 1e-7
        or not cover["valid"]
        or cover["minimum_clearance_m"] < 2 - 1e-7
        or domain_error > 1e-7
        or not np.array_equal(
            road.triangles, arrays["old_vertex_map"][previous[1].triangles]
        )
        or not np.array_equal(
            mesh.vertices[arrays["old_vertex_map"], :2], previous[1].vertices[:, :2]
        )
    ):
        raise ValueError("joint ground independent whole-surface replay failed")
    return {
        "maximum_normalized_grade": float(grade.max()),
        "whole_bridge_clearance": bridge,
        "whole_soil_underside_gap": soil_gap,
        "whole_roof_cover_to_soil_underside": cover,
        "maximum_old_road_lowering_m": float(-changed.min()),
        "domain_error_m2": domain_error,
        "unchanged_upper_contacts_and_tunnel": replay(model, previous, old_note),
    }


class BoundedGround(unittest.TestCase):
    def setUp(self):
        self.xy = np.array([[0.0, 0.0], [1, 0], [0, 1]])
        self.faces = np.array([[0, 1, 2]])
        self.frames = np.eye(2)[None]

    def test_joint_caps_match_analytic_constant_optimum(self):
        bounds = vertex_caps([0, 1, 2], [2.0, 2.0, 2.0])
        for weights in (np.ones(3), np.array([1.0, 2.0, 3.0])):
            z, report = fit(
                self.xy, self.faces, np.full(3, 3.0), self.frames, bounds, weights
            )
            self.assertTrue(report["valid"])
            np.testing.assert_allclose(z, 2, rtol=0, atol=1e-7)

    def test_crossing_barrier_edges_constrain_interior_not_just_vertices(self):
        xy = np.array([[-2.0, -1], [2, -1], [0, 2]])
        barrier = Mesh(
            np.c_[np.array([[-2.0, 1], [2, 1], [0, -2]]), np.ones(3)], self.faces
        )
        bounds, note = overlap_bounds(xy, self.faces, barrier, offset_m=-0.2)
        z, report = fit(
            xy, self.faces, np.full(3, 3.0), self.frames, bounds, np.ones(3)
        )
        self.assertTrue(report["valid"])
        self.assertGreater(note["rows"], 3)
        gap = vertical_clearance(Mesh(np.c_[xy, z], self.faces), barrier)
        self.assertGreaterEqual(gap["minimum_clearance_m"], 0.2 - 1e-7)

    def test_contradictory_bounds_never_return_unchecked_candidate(self):
        bounds = joined(
            vertex_caps([0], [0.0]),
            (np.zeros((1, 3), int), np.array([[-1.0, 0, 0]]), np.array([-1.0])),
        )
        z, report = fit(
            self.xy, self.faces, np.zeros(3), self.frames, bounds, np.ones(3)
        )
        self.assertIsNone(z)
        self.assertFalse(report["valid"])

    def test_lower_barrier_and_budget_exhaustion(self):
        barrier = Mesh(np.c_[self.xy, np.full(3, 2.0)], self.faces)
        bounds, _ = overlap_bounds(self.xy, self.faces, barrier, offset_m=0.3, sign=-1)
        z, report = fit(
            self.xy, self.faces, np.zeros(3), self.frames, bounds, np.ones(3)
        )
        self.assertTrue(report["valid"])
        np.testing.assert_allclose(z, 2.3, rtol=0, atol=1e-7)
        for options in ({"seconds": 0}, {"iterations": 0}):
            z, _report = fit(
                self.xy,
                self.faces,
                np.zeros(3),
                self.frames,
                bounds,
                np.ones(3),
                **options,
            )
            self.assertIsNone(z)
        with self.assertRaises(ValueError):
            overlap_bounds(self.xy, self.faces, barrier, max_pairs=0)

    def test_bad_weights_and_whole_face_gradient(self):
        bounds = vertex_caps([0, 1, 2], [5.0, 5.0, 5.0])
        target = np.array([0.0, 1.0, 1.0])
        before = target.copy()
        z, report = fit(self.xy, self.faces, target, self.frames, bounds, np.ones(3))
        self.assertTrue(report["valid"])
        self.assertLessEqual(
            np.linalg.norm(normal_gradients(self.xy, self.faces, z)), 1 + 1e-7
        )
        np.testing.assert_array_equal(target, before)
        for weights in (np.array([1.0, 0, 1]), np.array([1.0, np.nan, 1])):
            with self.assertRaises(ValueError):
                fit(self.xy, self.faces, target, self.frames, bounds, weights)


if __name__ == "__main__":
    unittest.main()
