"""Actual old end-cap query and independent metric/whole-interval controls."""

import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from stair_module_loops import actual_profile
from stair_module_sections import line_pieces, wall_strip, whole_line_pieces
from stair_module_trace import compare


def strip(x0, x1, y0=-1, y1=1):
    vertices = np.array([[x0, y0, 0], [x1, y0, 0], [x1, y1, 0], [x0, y1, 0]], float)
    return vertices, np.array([[0, 1, 2], [0, 2, 3]])


class MetricWholeLines(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = (
            Path(__file__).with_name("fixtures") / "stair-metric-end-profile-vienna.npz"
        )
        cls.meta = json.loads(path.with_suffix(".json").read_text())
        if digest(path) != cls.meta["data_sha256"]:
            raise ValueError("actual fixed end-profile bytes changed")
        with np.load(path, allow_pickle=False) as data:
            cls.data = {key: data[key] for key in data.files}

    def test_actual_half_profile_is_present_within_existing_metric_bound(self):
        d = self.data
        exact, _ = line_pieces(d["vertices"], d["faces"], d["line_xy"])
        query = shapely.LineString(d["line_xy"])
        self.assertAlmostEqual(query.difference(shapely.union_all(exact)).length, 0.9)
        mesh = Mesh(d["vertices"], d["faces"])
        sampled = SurfaceSampler(mesh, xy_tolerance_m=1e-7).sample(d["line_xy"])
        np.testing.assert_array_equal(sampled.points_enu_m, d["sampled"])
        pieces, parents = whole_line_pieces(d["vertices"], d["faces"], d["line_xy"])
        self.assertEqual(query.difference(shapely.union_all(pieces)).length, 0)
        points, ids = shapely.get_coordinates(pieces, return_index=True)
        distance = shapely.distance(
            shapely.points(points),
            shapely.polygons(d["vertices"][d["faces"][parents[ids]], :2]),
        )
        self.assertLessEqual(distance.max(), 1e-7)

    def test_original_cached_end_query_uses_the_complete_actual_cap(self):
        d, meta = self.data, self.meta
        key = meta["source_id"]
        state = {
            "cached_module_rows": {
                key: {"xy": meta["source_axis_xy_m"], "width_m": 1.8}
            },
            "road": {
                "body_vertices": d["vertices"],
                "top_faces": d["faces"],
                "face_sources": np.ones(len(d["faces"]), np.uint64),
            },
            "road_ground": np.ones(len(d["faces"]), bool),
            "report": {"candidate_source_ids": [key]},
        }
        sources = {
            key: {"xy_m": meta["source_axis_xy_m"], "mvt_xy": meta["source_nodes_mvt"]}
        }
        contact = actual_profile(state, sources, key, 3, interior=False)
        np.testing.assert_array_equal(contact["profile"], d["sampled"])
        self.assertEqual(contact["mvt_xy"], meta["original_end_mvt_xy"])

    def test_successful_exact_query_keeps_all_piece_and_parent_bytes(self):
        vertices, faces = strip(-3, 3)
        line = np.array([[-2, 0], [0, 0], [2, 0]], float)
        before = line_pieces(vertices, faces, line)
        after = whole_line_pieces(vertices, faces, line)
        np.testing.assert_array_equal(
            shapely.to_wkb(before[0]), shapely.to_wkb(after[0])
        )
        np.testing.assert_array_equal(before[1], after[1])

    def test_three_valid_samples_cannot_hide_a_missing_intermediate_interval(self):
        a, af = strip(-3, -2)
        b, bf = strip(-1, 3)
        vertices, faces = np.vstack([a, b]), np.vstack([af, bf + len(a)])
        line = np.array([[-3, 0], [0, 0], [3, 0]], float)
        samples = SurfaceSampler(Mesh(vertices, faces)).sample(line)
        self.assertTrue(np.isfinite(samples.points_enu_m).all())
        with self.assertRaisesRegex(ValueError, "missing intervals"):
            whole_line_pieces(vertices, faces, line)

    def test_metric_distance_is_not_an_unbounded_search_epsilon(self):
        vertices, faces = strip(-2, 2)
        for gap in (0.5e-7, 2e-7, 1e-3):
            line = np.array([[-1, 1 + gap], [1, 1 + gap]])
            if gap < 1e-7:
                pieces, _ = whole_line_pieces(vertices, faces, line)
                self.assertEqual(
                    shapely.LineString(line)
                    .difference(shapely.union_all(pieces))
                    .length,
                    0,
                )
            else:
                with self.assertRaises(ValueError):
                    whole_line_pieces(vertices, faces, line)

    def test_actual_whole_wall_and_height_disagreement_remain_separate(self):
        d = self.data
        cap = Mesh(d["vertices"], d["faces"])
        patch, _datum, _guard = wall_strip(cap, d["line_xy"][[0, -1]], False, 1e-7)
        self.assertAlmostEqual(patch.area, 1.8 * 0.3, places=6)
        for cast in (False, True):
            with self.assertRaisesRegex(ValueError, "heights disagree"):
                compare(
                    [
                        [(d["vertices"], d["faces"], None)],
                        [(d["vertices"] + [0, 0, 0.001], d["faces"], None)],
                    ],
                    d["line_xy"],
                    cast,
                )

    def test_query_budget_is_checked_before_geometry_work(self):
        vertices, faces = strip(-2, 2)
        with self.assertRaisesRegex(ValueError, "bounded whole"):
            whole_line_pieces(
                vertices, np.tile(faces, (16385, 1)), np.array([[-1, 0], [1, 0]])
            )


if __name__ == "__main__":
    unittest.main()
