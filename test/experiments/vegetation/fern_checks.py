"""Nested fern identities, independent normals/anchors and true saved partial replay."""

import json
import unittest
from unittest.mock import patch

import numpy as np
from fern_mesh import fern_mesh, pinna_mesh
from fern_mesh_ops import STEM, repeat, source
from fern_plan import prepare
from outshine_ref.vegetation import FrondGuide, PinnatePattern
from scipy.spatial.transform import Rotation


def crop(mesh, selected):
    ids = mesh["organ_ids"]
    mask = np.zeros(len(ids), bool)
    for i, j in selected:
        mask |= (ids[:, 0] == i) & (ids[:, 1] == (STEM if j == -1 else j))
    faces = mesh["faces"]
    fm = mask[faces].all(axis=1)
    indices = np.full(len(ids), -1, np.int32)
    indices[mask] = np.arange(np.count_nonzero(mask))
    return {
        "vertices": mesh["vertices"][mask],
        "faces": indices[faces[fm]],
        "normals": mesh["normals"].reshape(-1, 3, 3)[fm].reshape(-1, 3),
        "corner_uv": mesh["corner_uv"].reshape(-1, 3, 2)[fm].reshape(-1, 2),
        "organ_ids": ids[mask],
        "surface": mesh["surface"][fm],
    }


class FernChecks(unittest.TestCase):
    def test_equal_batch_individual_and_saved_subsets_in_every_array(self):
        plan = json.loads(json.dumps(prepare(221, 3, 3)))
        with (
            patch.object(
                np.random, "Generator", side_effect=AssertionError("cached RNG")
            ),
            patch.object(
                np.random, "default_rng", side_effect=AssertionError("cached RNG")
            ),
        ):
            pinna = pinna_mesh(plan, 4)
            full = fern_mesh(plan, pinna)
            one = fern_mesh(plan, pinna, method="individual")
            selected = [(2, 3), (0, 0), (1, -1)]
            part = fern_mesh(plan, pinna, selected=selected)
        for key, value in full.items():
            self.assertEqual(value.dtype, one[key].dtype)
            self.assertEqual(value.tobytes(), one[key].tobytes(), key)
        expected = crop(full, selected)
        for key, value in expected.items():
            self.assertEqual(value.dtype, part[key].dtype)
            self.assertEqual(value.tobytes(), part[key].tobytes(), key)
            self.assertFalse(np.shares_memory(part[key], pinna.get(key)))

    def test_budgets_precede_prototypes_guides_and_unrequested_guides_are_skipped(self):
        plan = prepare(221, 3, 3)
        with patch.object(
            PinnatePattern,
            "leaflet_prototype",
            side_effect=AssertionError("source work"),
        ):
            self.assertIsNone(pinna_mesh(plan, max_triangles=0))
        pinna = pinna_mesh(plan, 4)
        with patch.object(
            FrondGuide, "from_recipe", side_effect=AssertionError("guide work")
        ):
            self.assertIsNone(fern_mesh(plan, pinna, max_triangles=0))
        with patch.object(
            FrondGuide, "from_recipe", wraps=FrondGuide.from_recipe
        ) as counted:
            part = fern_mesh(plan, pinna, selected=[(1, 0)])
            self.assertEqual(counted.call_count, 1)
        self.assertEqual(len(part["faces"]), len(pinna["faces"]))
        complete = fern_mesh(plan, pinna)
        self.assertIsNone(
            fern_mesh(plan, pinna, max_triangles=len(complete["faces"]) - 1)
        )

    def test_near_field_id_sets_and_all_rachis_nodes_are_exact(self):
        plan = prepare(59, 3, 3)
        variants = [
            (pinna_mesh(plan, 6), 24),
            (pinna_mesh(plan, 2, rib_segments=3, rib_sides=4), 10),
        ]
        meshes = []
        for pinna, rib in variants:
            mesh = fern_mesh(plan, pinna, rib_segments=rib)
            meshes.append(mesh)
            anchor = np.flatnonzero((pinna["vertices"] == 0).all(axis=1))[0]
            for frond in plan["fronds"]:
                guide = FrondGuide.from_recipe(frond["guide"])
                frame = np.asarray(frond["frame"])
                for row in frond["pinnae"]:
                    local = guide.evaluate(np.asarray(row["origin"])[None])[0][0]
                    expected = (
                        sum(frame[:, k] * local[k] for k in range(3)) + frond["origin"]
                    )
                    subset = mesh["vertices"][
                        (mesh["organ_ids"][:, 0] == frond["id"])
                        & (mesh["organ_ids"][:, 1] == row["id"])
                    ]
                    np.testing.assert_array_equal(subset[anchor], expected)
            points = mesh["vertices"][mesh["faces"]]
            cross = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
            self.assertTrue(
                (
                    np.einsum("ij,ikj->ik", cross, mesh["normals"].reshape(-1, 3, 3))
                    > 0
                ).all()
            )
            np.testing.assert_allclose(
                np.linalg.norm(mesh["normals"], axis=1), 1, atol=8e-16
            )
        np.testing.assert_array_equal(
            np.unique(meshes[0]["organ_ids"], axis=0),
            np.unique(meshes[1]["organ_ids"], axis=0),
        )
        self.assertLess(len(meshes[1]["faces"]), len(meshes[0]["faces"]))

    def test_rigid_uniform_normals_against_independent_inverse_transpose(self):
        v = np.array(((0, 0, 0), (1, 0, 0), (0, 1, 0)), float)
        mesh = source(
            {
                "vertices": v,
                "faces": np.array(((0, 1, 2),)),
                "normals": np.tile((0.0, 0.0, 1.0), (3, 1)),
            },
            0,
            6,
        )
        frame = Rotation.from_rotvec((0.4, -0.2, 0.8)).as_matrix()
        result = repeat(
            mesh,
            np.array(((1.0, 2.0, 3.0),)),
            frame[None],
            np.array([0.25]),
            np.array([2]),
        )
        np.testing.assert_allclose(
            result["vertices"], v @ (0.25 * frame).T + (1, 2, 3), atol=5e-16
        )
        expected = np.linalg.solve((0.25 * frame).T, np.array((0.0, 0.0, 1.0)))
        expected /= np.linalg.norm(expected)
        np.testing.assert_allclose(
            result["normals"], np.tile(expected, (3, 1)), atol=4e-16
        )
        reflected = frame.copy()
        reflected[:, 0] *= -1
        with self.assertRaises(ValueError):
            repeat(mesh, np.zeros((1, 3)), reflected[None], np.ones(1), np.array([0]))
        with self.assertRaises(ValueError):
            repeat(mesh, np.zeros((1, 3)), frame[None], np.zeros(1), np.array([0]))


if __name__ == "__main__":
    unittest.main()
