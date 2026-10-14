"""Analytic ABI controls and real process/memory isolation checks for SDK calls."""

import json
from pathlib import Path
import subprocess
import time
import unittest

import numpy as np
import shapely

from esri_transport_oracle import reference
from outshine_ref.geometry import conform_triangles


class OracleChecks(unittest.TestCase):
    def test_public_version_and_layout(self):
        result = reference(dict(operation="version"))
        self.assertEqual(result["verified_version"], [3, 4, 12206])
        self.assertEqual(result["public_version_bytes"], 192)

    def test_interior_crossing_with_translation_reversal_and_canaries(self):
        base = np.array([[-2., 0., 0.], [2., 0., 0.], [0., -3., 0.], [0., 3., 0.]])
        vectors = [base, base[[1, 0, 3, 2]], base + [810000., -710000., 153.]]
        result = reference(dict(operation="edges", vectors=np.asarray(vectors).tolist()))
        self.assertTrue(result["output_canaries_verified"])
        for record, point in zip(result["results"], ([0., 0., 0.], [0., 0., 0.], [810000., -710000., 153.])):
            self.assertTrue(record["accepted"])
            np.testing.assert_allclose(record["candidate_xyz"], point, atol=1e-10, rtol=0)

    def test_sdk_half_open_endpoint_semantics(self):
        cases = [[[0., 0., 0.], [1., 0., 0.], [0., -1., 0.], [0., 1., 0.]],
                 [[0., 0., 0.], [1., 0., 0.], [1., -1., 0.], [1., 1., 0.]],
                 [[0., 0., 0.], [2., 0., 0.], [1., 0., 0.], [1., 2., 0.]]]
        result = reference(dict(operation="edges", vectors=cases))
        self.assertEqual([r["accepted"] for r in result["results"]], [True, False, False])

    def test_parallel_and_degenerate_reject_but_sdk_closest_mode_accepts_skew(self):
        cases = [[[0, 0, 0], [2, 0, 0], [1, 0, 0], [3, 0, 0]],
                 [[-1, 0, 0], [1, 0, 0], [0, -1, 1], [0, 1, 1]],
                 [[0, 0, 0], [0, 0, 0], [0, -1, 0], [0, 1, 0]]]
        records = reference(dict(operation="edges", vectors=cases))["results"]
        self.assertEqual([r["accepted"] for r in records], [False, True, False])
        np.testing.assert_allclose(records[1]["candidate_xyz"], [0., 0., 0.], atol=1e-12)

    def test_ray_modes_are_not_axis_numbers(self):
        vectors = [[[-1, 0, 0], [1, 0, 0], [0, -1, 1], [0, 1, 0]]]
        rejected = reference(dict(operation="ray", mode=0, vectors=vectors))["results"][0]
        self.assertFalse(rejected["accepted"])
        for mode, point in ((1, [0., 0., 0.]), (2, [0., 0., 1.])):
            record = reference(dict(operation="ray", mode=mode, vectors=vectors))["results"][0]
            self.assertTrue(record["accepted"])
            np.testing.assert_allclose(record["candidate_xyz"], point, atol=1e-12)
            self.assertAlmostEqual(record["parameter"], 1.)
        vectors[0][-1] = [0, 2, 0]
        record = reference(dict(operation="ray", mode=2, vectors=vectors))["results"][0]
        self.assertAlmostEqual(record["parameter"], .5)

    def test_offset_rectangle_analytic_area_and_repeat_stability(self):
        request = dict(operation="offset", xy=[[0, 0], [10, 0], [10, 6], [0, 6]], distance=1, repetitions=3)
        result = reference(request)
        first = result["results"][0]
        self.assertTrue(all(r == first for r in result["results"]))
        polygon = shapely.Polygon(first["sdk_vertices_xz"])
        self.assertEqual(polygon.area, 32.)
        self.assertTrue(polygon.equals(shapely.box(1, 1, 9, 5)))

    def test_native_pause_is_killed_and_reaped_by_parent(self):
        began = time.perf_counter()
        with self.assertRaises(subprocess.TimeoutExpired):
            reference(dict(operation="pause-test"), wall_limit_s=.45)
        self.assertLess(time.perf_counter() - began, 2.)

    def test_address_space_limit_denies_large_mapping(self):
        result = reference(dict(operation="memory-test"))
        self.assertTrue(result["address_space_limit_verified"])
        self.assertEqual(result["address_space_limit_bytes"], 2147483648)

    def test_sdk_tessellation_backend_preserves_courtyard_and_orientation(self):
        rings = [[[0, 0], [10, 0], [10, 10], [0, 10]], [[3, 3], [3, 7], [7, 7], [7, 3]]]
        result = reference(dict(operation="tessellation", rings=rings))
        points, faces = np.asarray(result["vertices"]), np.asarray(result["triangles"])
        p = points[faces, :2]
        u, v = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
        self.assertTrue(np.all(u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0] > 0))
        polygons = shapely.polygons(p)
        self.assertTrue(shapely.coverage_is_valid(polygons))
        self.assertTrue(shapely.coverage_union_all(polygons).equals(shapely.Polygon(rings[0], rings[1:])))
        self.assertEqual(sum(shapely.area(polygons)), 84.)
        edges, counts = np.unique(np.sort(np.vstack((faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]])), axis=1), axis=0, return_counts=True)
        self.assertLessEqual(counts.max(), 2)
        self.assertEqual(int((counts == 1).sum()), 8)
        self.assertEqual(result["combined_vertices"], 0)

    def test_collinear_sdk_raster_primitives_need_cad_adapter(self):
        domain = shapely.union_all([shapely.box(0, 0, 4, 4), shapely.box(8, 0, 12, 4), shapely.box(4, 1.75, 8, 2.25)])
        result = reference(dict(operation="tessellation", rings=[np.asarray(domain.exterior.coords)[:-1].tolist()]))
        xy, raw = np.asarray(result["vertices"])[:, :2], np.asarray(result["triangles"])
        p = xy[raw]
        u, v = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
        area = u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]
        self.assertEqual(int((area == 0).sum()), 2)
        faces, _ = conform_triangles(xy, raw[area > 0])
        triangles = shapely.polygons(xy[faces])
        self.assertTrue(shapely.coverage_is_valid(triangles))
        self.assertTrue(shapely.coverage_union_all(triangles).equals(domain))
        self.assertEqual(len(np.unique(faces)), len(xy))

    def test_python_only_cost_worker_does_not_load_sdk(self):
        result = reference(dict(operation="offset", backend="python", xy=[[0, 0], [10, 0], [10, 6], [0, 6]], distance=1))
        self.assertFalse(result["sdk_dependency_loaded"])
        self.assertTrue(shapely.Polygon(result["results"][0]["own_rings_xy"][0]).equals(shapely.box(1, 1, 9, 5)))


if __name__ == "__main__":
    result = unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(OracleChecks))
    if not result.wasSuccessful():
        raise SystemExit(1)
    output = Path("build/experiments/transport/esri-reference/oracle-checks.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(dict(status="pass", checks=result.testsRun,
                                      scope="Pinned scalar operations and Linux child limits, not universal startup/SDK/runtime deadline")) + "\n")
