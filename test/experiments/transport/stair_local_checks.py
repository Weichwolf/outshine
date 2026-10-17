"""Bounded resident working sets and complete geometric terrain dependencies."""

import unittest

import numpy as np
import shapely
from outshine_ref.contracts import LocalFrame, Mesh
from outshine_ref.geometry.surface_patches import SurfacePatch, SurfacePatches
from stair_local_assets import road_asset, road_window
from stair_local_context import neighbourhood, selected_keys


def asset(index, *, x=None, sources=1):
    x = index * 100.0 if x is None else x
    xy = np.array([[x, 0], [x + 2, 0], [x + 2, 2], [x, 2]])
    xyz = np.vstack([np.c_[xy, np.ones(4)], np.c_[xy, np.zeros(4)]])
    top = np.array([[0, 1, 2], [0, 2, 3]])
    walls = []
    for a, b in ((0, 1), (1, 2), (2, 3), (3, 0)):
        walls.extend([[a, a + 4, b], [b, a + 4, b + 4]])
    body = np.vstack([top, top[:, ::-1] + 4, walls])
    data = {
        "body_vertices": xyz,
        "body_faces": body,
        "top_faces": top,
        "bottom_faces": top + 4,
        "owners": np.ones((2, sources), bool),
        "ground": np.ones(2, bool),
    }
    return road_asset(
        str(index), [str(index) + ":" + str(i) for i in range(sources)], [], data
    )


def terrain():
    xy, faces = [], []
    for x in (0, 100, 200):
        first = len(xy)
        xy.extend([[x, 0], [x + 100, 0], [x + 100, 100], [x, 100]])
        faces.extend([[first, first + 1, first + 2], [first, first + 2, first + 3]])
    xy, inverse = np.unique(xy, axis=0, return_inverse=True)
    mesh = Mesh(np.c_[xy, np.zeros(len(xy))], inverse[np.array(faces)])
    patch = SurfacePatch(
        "ground",
        LocalFrame("test-local-up0"),
        "terrain_top",
        mesh,
        {"ground": np.zeros(6, bool), "constructed": np.zeros(6, bool)},
    )
    return SurfacePatches(patch)


class LocalRoadAssets(unittest.TestCase):
    def test_global_source_count_above_64_has_no_global_bitmask(self):
        assets = tuple(asset(i) for i in range(70))
        road, ground, keys, rows, work = road_window(
            assets, shapely.box(6900, 0, 6902, 2)
        )
        self.assertEqual(keys, ["69:0"])
        self.assertEqual(work["local_sources"], 1)
        self.assertEqual(work["resident_assets_tested"], 70)
        self.assertEqual(rows, {})
        np.testing.assert_array_equal(road["face_sources"], np.ones(2, np.uint64))
        self.assertTrue(ground.all())

    def test_local_65_sources_fail_before_a_mask_can_wrap(self):
        assets = (asset(0, x=0, sources=32), asset(1, x=0, sources=32), asset(2, x=0))
        with self.assertRaisesRegex(ValueError, "source working budget"):
            road_window(assets, shapely.box(0, 0, 2, 2))
        with self.assertRaises(ValueError):
            road_window(assets, shapely.box(0, 0, 2, 2), max_sources=65)

    def test_immutable_resident_arrays_and_bounded_body_work(self):
        old = asset(0)
        with self.assertRaises(ValueError):
            old.data["body_vertices"].flags.writeable = True
        with self.assertRaises(TypeError):
            old.data["owners"] = np.zeros((2, 1), bool)
        with self.assertRaisesRegex(ValueError, "body face"):
            road_window([old], shapely.box(0, 0, 2, 2), max_faces=1)
        road, _ground, _keys, _rows, _work = road_window([old], shapely.box(0, 0, 2, 2))
        road["body_vertices"][0, 0] = 99
        self.assertEqual(old.data["body_vertices"][0, 0], 0)


class LocalTerrainDependencies(unittest.TestCase):
    @staticmethod
    def contacts():
        low = Mesh(
            np.array([[80, 80, 0], [180, 80, 0], [180, 90, 0], [80, 90, 0]]),
            np.array([[0, 1, 2], [0, 2, 3]]),
        )
        return [(low, low)]

    def test_pier_closure_and_five_metre_auxiliary_ground_are_complete(self):
        view, contacts = terrain(), self.contacts()
        selected, work = neighbourhood(
            view, shapely.LineString([[5, 40], [5, 60]]), contacts
        )
        foot = shapely.box(80, 80, 180, 90)
        self.assertTrue(set(view.nearby(foot, 0).face_keys) <= set(selected.face_keys))
        self.assertEqual(work["completed_pier_groups"], [0])
        self.assertGreaterEqual(work["auxiliary_ground_query"]["returned_faces"], 4)
        scanned, _note = neighbourhood(
            view, shapely.LineString([[5, 40], [5, 60]]), contacts, method="scan"
        )
        self.assertEqual(selected.face_keys, scanned.face_keys)

    def test_missing_local_faces_cannot_silently_remove_a_required_neighbour(self):
        view = terrain()
        incomplete = view.faces([("ground", 0)])
        with self.assertRaisesRegex(ValueError, "omitted required terrain"):
            selected_keys(
                view,
                incomplete,
                np.ones(1, bool),
                shapely.box(50, 45, 60, 55),
                self.contacts(),
            )
        before = view.patches[0].mesh.vertices.copy()
        with self.assertRaisesRegex(ValueError, "budget"):
            neighbourhood(view, shapely.box(0, 0, 20, 20), self.contacts(), max_faces=1)
        np.testing.assert_array_equal(before, view.patches[0].mesh.vertices)


if __name__ == "__main__":
    unittest.main()
