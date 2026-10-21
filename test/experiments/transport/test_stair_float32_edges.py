"""Frozen real alias stars, preserved bands and geometric/topological refusals."""

import json
import unittest
from collections import defaultdict
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from stair_float32_edges import contract_alias_edges


def fixture(name="alias"):
    path = Path(__file__).with_name("fixtures") / f"mvt_stair_float32_{name}_star.json"
    data = json.loads(path.read_text())
    return (
        np.array(data["points_xyz_m"]),
        np.array(data["faces"]),
        np.array(data["owners"]),
        np.array(data["steps"]),
    )


def signed(points, faces):
    xy = points[faces, :2]
    a, b = xy[:, 1] - xy[:, 0], xy[:, 2] - xy[:, 0]
    return a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]


def alias_pair(points):
    groups = defaultdict(list)
    for i, point in enumerate(points):
        groups[(*point[:2].astype(np.float32), point[2])].append(i)
    return next(v for v in groups.values() if len(v) == 2)


class ActualFloat32Edges(unittest.TestCase):
    def test_real_stars_lose_only_degenerate_gpu_faces_and_keep_each_band(self):
        for name, expected in (("alias", 1), ("riser", 2)):
            with self.subTest(name=name):
                points, faces, owners, steps = fixture(name)
                before = tuple(a.copy() for a in (points, faces, owners, steps))
                self.assertGreater(
                    np.count_nonzero(
                        signed(points.astype("f4").astype(float), faces) == 0
                    ),
                    0,
                )
                p, f, o, s, note = contract_alias_edges(points, faces, owners, steps)
                self.assertEqual(note["accepted_edges"], expected)
                self.assertEqual(note["removed_faces"], 2 * expected)
                self.assertTrue(np.all(signed(p, f) > 0))
                self.assertTrue(np.all(signed(p.astype("f4").astype(float), f) > 0))
                for level in np.unique(steps):
                    old = shapely.union_all(
                        shapely.polygons(points[faces[steps == level], :2])
                    )
                    new = shapely.union_all(shapely.polygons(p[f[s == level], :2]))
                    self.assertLess(old.symmetric_difference(new).area, 1e-12)
                self.assertEqual(set(o), set(owners))
                sampled = SurfaceSampler(Mesh(p, f), xy_tolerance_m=0).sample(
                    points[:, :2]
                )
                self.assertFalse(any(sampled.reasons))
                np.testing.assert_allclose(
                    sampled.points_enu_m[:, 2], points[:, 2], rtol=0, atol=1e-10
                )
                for a, b in zip(before, (points, faces, owners, steps), strict=True):
                    np.testing.assert_array_equal(a, b)

    def test_source_and_step_membership_boundaries_are_preserved(self):
        for field in ("owner", "step"):
            with self.subTest(field=field):
                p, f, o, s = fixture()
                if field == "owner":
                    o[0] = 16
                else:
                    s[0] = 100
                *_, note = contract_alias_edges(p, f, o, s)
                self.assertEqual(note["accepted_edges"], 0)

    def test_other_height_planes_do_not_become_a_flat_alias_patch(self):
        p, f, o, s = fixture()
        pair = alias_pair(p)
        p[next(i for i in range(len(p)) if p[i, 2] != p[pair[0], 2]), 2] += 0.01
        *_, note = contract_alias_edges(p, f, o, s)
        self.assertEqual(note["accepted_edges"], 0)
        self.assertIn("different_affine_height_planes", note["refusals"])

    def test_an_actual_boundary_edge_is_not_contracted(self):
        p, f, o, s = fixture()
        a, b = alias_pair(p)
        removed = np.flatnonzero(np.sum(np.isin(f, [a, b]), axis=1) == 2)[0]
        keep = np.arange(len(f)) != removed
        *_, note = contract_alias_edges(p, f[keep], o[keep], s[keep])
        self.assertEqual(note["accepted_edges"], 0)

    def test_disconnected_gpu_aliases_do_not_create_connectivity(self):
        p, _f, _o, _s = fixture()
        a, b = p[alias_pair(p)]
        points = np.array(
            [a, a + [1, 0, 0], a + [0, 1, 0], b, b - [1, 0, 0], b - [0, 1, 0]]
        )
        faces = np.array([[0, 1, 2], [3, 4, 5]])
        *_, note = contract_alias_edges(
            points, faces, np.ones(2, dtype=int), np.ones(2, dtype=int)
        )
        self.assertEqual(note["accepted_edges"], 0)

    def test_different_logical_heights_are_not_merged_by_gpu_rounding(self):
        p, f, o, s = fixture()
        _a, b = alias_pair(p)
        p[b, 2] = np.nextafter(p[b, 2], np.inf)
        *_, note = contract_alias_edges(p, f, o, s)
        self.assertEqual(note["accepted_edges"], 0)

    def test_survivor_numbering_does_not_change_the_preserved_footprint(self):
        p, f, o, s = fixture()
        old = shapely.union_all(shapely.polygons(p[f, :2]))
        p, f, _o, _s, note = contract_alias_edges(p[::-1].copy(), len(p) - 1 - f, o, s)
        self.assertEqual(note["accepted_edges"], 1)
        self.assertLess(
            old.symmetric_difference(
                shapely.union_all(shapely.polygons(p[f, :2]))
            ).area,
            1e-12,
        )
        self.assertTrue(np.all(signed(p.astype("f4").astype(float), f) > 0))

    def test_budget_refusal_keeps_the_original_input(self):
        arrays = fixture()
        before = tuple(a.copy() for a in arrays)
        with self.assertRaisesRegex(ValueError, "budget"):
            contract_alias_edges(*arrays, max_alias_pairs=0)
        for a, b in zip(before, arrays, strict=True):
            np.testing.assert_array_equal(a, b)


if __name__ == "__main__":
    unittest.main()
