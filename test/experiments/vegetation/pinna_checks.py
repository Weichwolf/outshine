"""Independent edge, finite error, identity, partial replay and refusal controls."""

import json
import unittest
from unittest.mock import patch

import numpy as np
from fern_checks import crop
from fern_mesh import fern_mesh, pinna_mesh
from fern_plan import prepare
from pinna_boundary import (
    boundary_product,
    correspondence,
    group_bounds,
    part,
    select,
    snapshot,
)
from pinna_surface import domain


def boundary(mesh):
    f = mesh["faces"]
    edges = np.sort(np.concatenate((f[:, :2], f[:, 1:], f[:, ::2])), axis=1)
    unique, counts = np.unique(edges, axis=0, return_counts=True)
    points = mesh["vertices"][unique[counts == 1]]
    return sorted(tuple(sorted(tuple(v) for v in edge)) for edge in points)


def fan():
    vertices = np.array(
        ((-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0), (0, 0, 0.2)), float
    )
    faces = np.array(((0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4)), np.int32)
    return {
        "vertices": vertices,
        "faces": faces,
        "normals": np.tile((0.0, 0.0, 1.0), (faces.size, 1)),
        "corner_uv": vertices[faces.ravel(), :2].copy(),
        "organ_ids": np.zeros((len(vertices), 1), np.uint32),
        "surface": np.full(len(faces), 6, np.uint8),
    }


class PinnaChecks(unittest.TestCase):
    def test_known_pyramid_bound_and_strict_selector_fallback(self):
        source = fan()
        coarse = boundary_product(source)
        self.assertEqual(len(coarse["faces"]), 2)
        self.assertEqual(boundary(source), boundary(coarse))
        transfer = correspondence(source, coarse)
        for mesh in (source, coarse):
            mesh["organ_ids"] = np.zeros((len(mesh["vertices"]), 3), np.uint32)
        records = group_bounds(source, coarse, transfer)
        self.assertAlmostEqual(
            records[0]["displacement_bound_m"], 0.2 + 1e-10, places=14
        )
        self.assertEqual(select(records, 1, 3), {(0, 0)})
        self.assertEqual(select(records, 1, 4), set())
        self.assertEqual(select(records, 1, 3, projection_gain=2), set())
        # Independent dense fan samples: distance to the coarser z=0 surface.
        w = np.array(
            [
                (a, b, 1 - a - b)
                for a in np.linspace(0, 1, 21)
                for b in np.linspace(0, 1 - a, 21)
            ]
        )
        samples = np.einsum("ni,fij->fnj", w, source["vertices"][source["faces"]])
        self.assertLessEqual(
            float(abs(samples[:, :, 2]).max()), records[0]["displacement_bound_m"]
        )

    def test_cached_real_source_edges_normals_uvs_and_partial_replay(self):
        plan = json.loads(json.dumps(prepare(221, 3, 3)))
        source = pinna_mesh(plan, 4, rib_segments=3, rib_sides=4)
        saved = {k: v.copy() for k, v in source.items()}
        with patch.object(
            np.random, "default_rng", side_effect=AssertionError("cached RNG")
        ):
            coarse = boundary_product(source)
            a, b = fern_mesh(plan, source), fern_mesh(plan, coarse)
            selected = [(2, 3), (0, 0), (1, -1)]
            partial = fern_mesh(plan, coarse, selected=selected)
        for k, v in crop(b, selected).items():
            np.testing.assert_array_equal(v, partial[k])
        self.assertLess(len(b["faces"]), len(a["faces"]))
        for i in range(len(plan["pinnules"])):
            m = part(source, (source["organ_ids"][source["faces"][:, 0], 0] == i))
            n = part(coarse, (coarse["organ_ids"][coarse["faces"][:, 0], 0] == i))
            self.assertEqual(boundary(m), boundary(n))
            self.assertTrue(
                domain(m["vertices"], m["faces"]).equals(
                    domain(n["vertices"], n["faces"])
                )
            )
            for j, vertex in enumerate(n["vertices"]):
                old = np.flatnonzero(np.all(m["vertices"] == vertex, axis=1))[0]
                for key, width in (("normals", 3), ("corner_uv", 2)):
                    expected = m[key].reshape(-1, width)[m["faces"].ravel() == old][0]
                    actual = n[key].reshape(-1, width)[n["faces"].ravel() == j]
                    np.testing.assert_array_equal(
                        actual, np.tile(expected, (len(actual), 1))
                    )
        for key in source:
            np.testing.assert_array_equal(source[key], saved[key])
            self.assertFalse(np.shares_memory(source[key], coarse[key]))
        transfer = correspondence(source, coarse)
        records = group_bounds(a, b, transfer)
        self.assertEqual(len(records), sum(len(r["pinnae"]) for r in plan["fronds"]))
        self.assertEqual(
            {tuple(v) for v in a["organ_ids"]}, {tuple(v) for v in b["organ_ids"]}
        )

    def test_snapshot_retains_all_ribs_and_unselected_leaves(self):
        plan = prepare(221, 3, 3)
        source = pinna_mesh(plan, 4)
        a, b = fern_mesh(plan, source), fern_mesh(plan, boundary_product(source))
        product = snapshot(a, b, {(1, 2)})
        for key, values in crop(a, [(0, 0), (1, -1), (2, 3)]).items():
            np.testing.assert_array_equal(
                values, crop(product, [(0, 0), (1, -1), (2, 3)])[key]
            )
        ribs_a = part(a, a["surface"] != 6)
        ribs_b = part(product, product["surface"] != 6)
        for key in ribs_a:
            np.testing.assert_array_equal(ribs_a[key], ribs_b[key])
        for key in a:
            np.testing.assert_array_equal(a[key], snapshot(a, b, set())[key])
        with self.assertRaises(ValueError):
            snapshot(a, b, {(50, 50)})

    def test_hard_corner_and_degenerate_projection_are_not_silently_lost(self):
        for key in ("normals", "corner_uv"):
            mesh = fan()
            mesh[key][2, 0] += 0.1
            with self.assertRaisesRegex(ValueError, "hard corner"):
                boundary_product(mesh)
        mesh = fan()
        mesh["vertices"][:, 0] = 0
        with self.assertRaises(ValueError):
            boundary_product(mesh)
        for scale, ppm, gain, error in (
            (float("nan"), 1, 1, 0.75),
            (1, 0, 1, 0.75),
            (1, 100, 0.5, 0.75),
            (1, 100, 1, -1),
        ):
            with self.assertRaises(ValueError):
                select([], scale, ppm, gain, error)


if __name__ == "__main__":
    unittest.main()
