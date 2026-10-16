"""Analytic direction flow, independent FK and rigid demanded organ controls."""

import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from crown_posture import pose_group, pose_rows, posture
from crown_shoots import CrownShoots
from posture_mesh import pose_wood
from posture_replay import read_pose
from scipy.integrate import solve_ivp
from scipy.spatial.transform import Rotation


def serial_reference(points, parents, mobility, target, curvature, cap):
    moved, matrices = points.copy(), np.tile(np.eye(3), (len(points), 1, 1))
    for node in range(1, len(points)):
        parent = parents[node]
        edge = matrices[parent] @ (points[node] - points[parent])
        length = np.linalg.norm(edge)
        tangent = edge / length
        solved = solve_ivp(
            lambda _, d, coefficient=curvature * mobility[node]: (
                coefficient * (target - np.dot(target, d) * d)
            ),
            (0, length),
            tangent,
            rtol=2e-11,
            atol=1e-12,
        ).y[:, -1]
        axis = np.cross(tangent, solved)
        sine = np.linalg.norm(axis)
        angle = min(np.arctan2(sine, np.dot(tangent, solved)), cap)
        rotation = np.eye(3)
        if sine > 1e-12:
            u = axis / sine
            cross = np.array(((0, -u[2], u[1]), (u[2], 0, -u[0]), (-u[1], u[0], 0)))
            rotation += np.sin(angle) * cross + (1 - np.cos(angle)) * (cross @ cross)
        matrices[node] = rotation @ matrices[parent]
        moved[node] = moved[parent] + rotation @ edge
    return moved, matrices


class PostureChecks(unittest.TestCase):
    def setUp(self):
        self.points = np.array(
            ((0, 0, 0), (0, 0, 1), (1, 0, 1), (0, 1, 1), (2, 0, 1), (0, 2, 1)), float
        )
        self.parents = np.array((-1, 0, 1, 1, 2, 3), np.int32)
        self.mobility = np.array((0, 0, 0.7, 1, 0.5, 0.8))
        self.target = np.array((0, 0, -1.0))

    def test_matches_independent_ode_and_serial_branch_propagation(self):
        got = posture(self.points, self.parents, self.mobility, self.target, 0.2)
        expected = serial_reference(
            self.points, self.parents, self.mobility, self.target, 0.2, 0.35
        )
        for a, b in zip(got, expected):
            np.testing.assert_allclose(a, b, atol=2e-11)
        moved, matrices = got
        np.testing.assert_allclose(np.linalg.det(matrices), 1, atol=2e-15)
        np.testing.assert_allclose(
            matrices @ matrices.transpose(0, 2, 1),
            np.tile(np.eye(3), (6, 1, 1)),
            atol=2e-15,
        )
        np.testing.assert_allclose(
            np.linalg.norm(moved[1:] - moved[self.parents[1:]], axis=1), 1, atol=2e-15
        )
        np.testing.assert_array_equal(moved[:2], self.points[:2])

    def test_exact_direction_flow_under_subdivision_before_cap(self):
        for count in (1, 2, 8, 32):
            p = np.zeros((count + 1, 3))
            p[:, 0] = np.linspace(0, 1, count + 1)
            moved, r = posture(
                p, np.arange(-1, count), np.ones(count + 1), self.target, 0.2
            )
            expected_angle = 2 * np.arctan(np.exp(-0.2))
            np.testing.assert_allclose(
                r[-1] @ (1, 0, 0),
                (np.sin(expected_angle), 0, -np.cos(expected_angle)),
                atol=1e-14,
            )
            self.assertEqual(moved.shape, p.shape)

    def test_rotation_translation_equivariance_and_parent_inheritance(self):
        moved, r = posture(self.points, self.parents, self.mobility, self.target, 0.3)
        q = Rotation.from_rotvec((0.7, -0.3, 0.5)).as_matrix()
        offset = np.array((13, -5, 2))
        other, s = posture(
            self.points @ q.T + offset,
            self.parents,
            self.mobility,
            q @ self.target,
            0.3,
        )
        np.testing.assert_allclose(other, moved @ q.T + offset, atol=2e-14)
        np.testing.assert_allclose(s, q @ r @ q.T, atol=2e-14)

    def test_zero_mask_zero_curvature_parallel_and_antiparallel(self):
        for k, m in ((0, self.mobility), (0.4, np.zeros(6))):
            a, r = posture(self.points, self.parents, m, self.target, k)
            np.testing.assert_array_equal(a, self.points)
            np.testing.assert_array_equal(r, np.tile(np.eye(3), (6, 1, 1)))
        p = np.array(((0, 0, 0), (0, 0, 1), (0, 0, 2)), float)
        for target in (self.target, -self.target):
            a, r = posture(p, np.array((-1, 0, 1)), np.ones(3), target, 1)
            np.testing.assert_array_equal(a, p)
            np.testing.assert_array_equal(r, np.tile(np.eye(3), (3, 1, 1)))

    def test_turn_cap_and_invalid_sources(self):
        p = np.array(((0, 0, 0), (1, 0, 0)), float)
        _, r = posture(p, np.array((-1, 0)), np.ones(2), self.target, 2, max_turn=0.1)
        self.assertAlmostEqual(np.arccos((r[1] @ (1, 0, 0))[0]), 0.1)
        for kw in (
            {"parents": np.array((-1, 2, 1, 1, 2, 3))},
            {"mobility": np.full(6, 1.1)},
            {"curvature": -1},
            {"direction": (0, 0, 2)},
            {"positions": np.zeros((6, 3))},
            {"max_turn": 1},
        ):
            args = {
                "positions": self.points,
                "parents": self.parents,
                "mobility": self.mobility,
                "direction": self.target,
                "curvature": 0.3,
            }
            args.update(kw)
            with self.assertRaises(ValueError):
                posture(**args)

    def test_demanded_groups_keep_ids_and_rigid_petiole_geometry(self):
        variants = np.arange(32, dtype=np.uint8) % 8
        crown = CrownShoots(self.points, self.parents, variants, foliage_depth_m=0.1)
        moved, r = posture(self.points, self.parents, self.mobility, self.target, 0.3)
        node = int(crown.hosts[-1])
        with patch(
            "outshine_ref.vegetation.Shoot.detail",
            side_effect=AssertionError("premature detail"),
        ):
            self.assertIsNone(pose_group(crown, moved, r, node, 0))
        self.assertFalse(crown._groups)
        rows = pose_group(crown, moved, r, node, 100)
        self.assertEqual(list(crown._groups), [node])
        original = crown.group(node, 100)
        for k in ("ids", "variants", "owner_nodes"):
            np.testing.assert_array_equal(rows[k], original[k])
        np.testing.assert_allclose(
            rows["origins"] - crown.petiole_m * rows["frames"][:, :, 1],
            rows["nodes"],
            atol=2e-15,
        )
        np.testing.assert_allclose(np.linalg.det(rows["frames"]), 1, atol=2e-15)
        restored = pose_rows(
            rows, rows["owner_nodes"], moved, self.points, r.transpose(0, 2, 1)
        )
        for k in ("origins", "frames", "nodes"):
            np.testing.assert_allclose(restored[k], original[k], atol=2e-15)

    def test_rigid_mesh_area_normals_and_cross_owner_rejection(self):
        source = np.array(((0, 0, 0), (1, 0, 0)), float)
        mesh = {
            "vertices": np.array(((0, 0, 0), (0, 1, 0), (1, 0, 0)), float),
            "faces": np.array(((0, 2, 1),)),
            "normals": np.tile((0, 0, 1.0), (3, 1)),
        }
        q = np.tile(Rotation.from_rotvec((0.2, 0.4, 0.3)).as_matrix(), (2, 1, 1))
        transformed = pose_wood(mesh, np.zeros(3, int), source, source + 2, q)
        a = transformed["vertices"][transformed["faces"][0]]
        np.testing.assert_allclose(
            np.cross(a[1] - a[0], a[2] - a[0]), transformed["normals"][0], atol=1e-15
        )
        with self.assertRaises(ValueError):
            pose_wood(mesh, np.array((0, 1, 0)), source, source + 2, q)

    def test_saved_pose_rejects_different_source_recipe_and_corruption(self):
        header = {
            "format": "spatial-crown-posture-1",
            "source_identity": "source-a",
            "direction": [0, 0, -1],
            "curvature_rad_per_m": 0.25,
            "max_turn_rad": 0.35,
        }
        digest = hashlib.sha256(json.dumps(header, sort_keys=True).encode())
        values = {
            "source": self.points,
            "parents": self.parents,
            "mobility": self.mobility,
        }
        for a in values.values():
            digest.update(a.view(np.uint8))
        header["identity"] = digest.hexdigest()
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.vegetation-posture-"
        ) as tmp:
            path = Path(tmp)
            np.savez_compressed(path / "posture.npz", **values)
            header["payload_sha256"] = hashlib.sha256(
                (path / "posture.npz").read_bytes()
            ).hexdigest()

            def write(h):
                (path / "posture.json").write_text(json.dumps(h))

            write(header)
            actual, _ = read_pose(path, "source-a")
            self.assertEqual(actual["identity"], header["identity"])
            with self.assertRaises(ValueError):
                read_pose(path, "source-b")
            write({**header, "curvature_rad_per_m": 0.3})
            with self.assertRaises(ValueError):
                read_pose(path, "source-a")
            write({**header, "format": "future-unimplemented-pose"})
            with self.assertRaises(ValueError):
                read_pose(path, "source-a")
            write(header)
            with (path / "posture.npz").open("ab") as f:
                f.write(b"changed")
            with self.assertRaises(ValueError):
                read_pose(path, "source-a")


if __name__ == "__main__":
    unittest.main()
