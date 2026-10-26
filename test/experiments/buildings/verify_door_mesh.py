"""Actual triangle solid, horizontal rays and actor intersections across door states."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import manifold3d as manifold
import numpy as np
import shapely
from door_assembly import mesh as assembly_mesh
from door_assembly import prepare
from door_frame import frame
from door_mesh import expand, template
from door_state_verification import check
from furniture_collision import furnishing_solid
from outshine_ref.buildings.interiors.doors import DoorLeaf, pose
from structural_shell_geometry import native


class DoorMesh(unittest.TestCase):
    def test_angle_changes_reuse_prepared_static_frames_and_local_leaf_geometry(self):
        leaf = DoorLeaf("fixed", (0, 0), (1, 0), (0, 1), 0.16)
        selected = ({"leaf": leaf, "storey": 0},)
        fixed = prepare(selected)
        expected = [assembly_mesh(selected, angle) for angle in (0.0, 45.0, 90.0)]
        before = {key: value.copy() for key, value in fixed[0][1].items()}
        with (
            patch(
                "door_assembly.frame",
                side_effect=AssertionError("Rebuilt static frame"),
            ),
            patch(
                "door_assembly.template",
                side_effect=AssertionError("Rebuilt local leaf"),
            ),
        ):
            for angle, reference in zip((0.0, 45.0, 90.0), expected, strict=True):
                actual = assembly_mesh(selected, angle, fixed=fixed)
                for key in actual:
                    np.testing.assert_array_equal(actual[key], reference[key])
        for key, value in before.items():
            np.testing.assert_array_equal(fixed[0][1][key], value)
        with self.assertRaises(ValueError):
            assembly_mesh(
                ({"leaf": replace(leaf, width_m=1.3), "storey": 0},), 45.0, fixed=fixed
            )

    def test_assembled_state_verifier_keeps_open_and_closed_queries_distinct(self):
        leaf = DoorLeaf("door/leaf", (0, 0), (1, 0), (0, 1), 0.16)
        opened, proof = check(
            manifold.Manifold(), [{"leaf": leaf, "storey": 0}], -1.0, 4.0
        )
        self.assertEqual(proof[0]["paired_state_rays"], 39)
        self.assertEqual(proof[0]["closed_oblique_rays"], 65)
        self.assertTrue(proof[0]["closed_actor_rejected"])
        self.assertFalse(opened.ray_cast([0, -0.5, 1.2], [0, 0.5, 1.2]))

    def test_room_face_frame_blocks_oblique_privacy_rays_without_blocking_opening(self):
        for side in (-1, 1):
            leaf = DoorLeaf("door/leaf", (0, 0), (1, 0), (0, 1), 0.16, hinge_side=side)
            fixed = template(leaf)
            framing = furnishing_solid(frame(leaf))
            closed = framing + native(expand(leaf, fixed, 0))
            opened = framing + native(expand(leaf, fixed, 90))
            for x in np.linspace(-0.549, 0.549, 13):
                for slope in (-4.0, -1.0, 0.0, 1.0, 4.0):
                    # All rays cross the actual inner wall-face aperture at x.
                    start = [x - 0.56 * slope, -0.5, 1.2]
                    end = [x + 0.44 * slope, 0.5, 1.2]
                    self.assertTrue(closed.ray_cast(start, end))
            for x in np.linspace(-0.54, 0.54, 13):
                self.assertFalse(opened.ray_cast([x, -0.5, 1.2], [x, 0.5, 1.2]))
            for angle in np.linspace(0, 90, 181):
                self.assertLess(
                    (framing ^ native(expand(leaf, fixed, float(angle)))).volume(),
                    1e-10,
                )
            # The frame closes an actual leak at the leaf overlap's oblique edge.
            a, b = [0.53 - 0.56 * 4, -0.5, 1.2], [0.53 + 0.44 * 4, 0.5, 1.2]
            self.assertFalse(native(expand(leaf, fixed, 0)).ray_cast(a, b))
            self.assertTrue(closed.ray_cast(a, b))

    def test_rigid_mesh_has_closed_positive_volume_and_matches_plan(self):
        for inward in ((0.0, 1.0), (0.0, -1.0)):
            for side in (-1, 1):
                leaf = DoorLeaf(
                    "door/leaf", (3.0, 7.0), (1.0, 0.0), inward, 2.0, hinge_side=side
                )
                fixed = template(leaf)
                before = fixed["vertices"].copy()
                for angle in (0.0, 17.25, 45.0, 90.0):
                    mesh = expand(leaf, fixed, angle)
                    body = native(mesh)
                    self.assertEqual(body.status(), manifold.Error.NoError)
                    self.assertAlmostEqual(body.volume(), 1.15 * 0.04 * 2.14)
                    projected = shapely.MultiPoint(mesh["vertices"][:, :2]).convex_hull
                    self.assertLess(
                        projected.symmetric_difference(pose(leaf, angle)).area, 1e-12
                    )
                    self.assertEqual(len(mesh["faces"]), 12)
                    self.assertAlmostEqual(mesh["vertices"][:, 2].min(), 2.005)
                    self.assertAlmostEqual(mesh["vertices"][:, 2].max(), 4.145)
                np.testing.assert_array_equal(fixed["vertices"], before)
                self.assertFalse(fixed["vertices"].flags.writeable)

    def test_closed_leaf_blocks_portal_rays_and_actor_open_leaf_clears_them(self):
        for side in (-1, 1):
            leaf = DoorLeaf(
                "door/leaf", (0.0, 0.0), (1.0, 0.0), (0.0, 1.0), 0.16, hinge_side=side
            )
            fixed = template(leaf)
            closed, opened = (native(expand(leaf, fixed, angle)) for angle in (0, 90))
            for x in np.linspace(-0.54, 0.54, 13):
                for z in (0.20, 1.20, 2.20):
                    self.assertTrue(closed.ray_cast([x, -0.5, z], [x, 0.5, z]))
                    self.assertFalse(opened.ray_cast([x, -0.5, z], [x, 0.5, z]))
            # Independent solid actor, radius .30 and height 1.90 metres.
            actor = manifold.Manifold.cylinder(1.90, 0.30, circular_segments=64)
            for y in np.linspace(-0.4, 0.5, 19):
                occupant = actor.translate([0.0, y, 0.16])
                self.assertLess((opened ^ occupant).volume(), 1e-12)
            self.assertGreater(
                (closed ^ actor.translate([0.0, 0.1, 0.16])).volume(), 0.01
            )
            # Wrong state binding is detected by the same queries.
            self.assertTrue(
                native(expand(leaf, fixed, 0)).ray_cast([0, -0.5, 1.2], [0, 0.5, 1.2])
            )
            self.assertEqual(leaf.id, "door/leaf")

    def test_vertical_datum_changes_only_height(self):
        leaf = DoorLeaf("leaf", (0, 0), (1, 0), (0, 1), 0.16)
        fixed = template(leaf)
        ground = expand(leaf, fixed, 34.0)
        upper = expand(replace(leaf, floor_z_m=3.16), fixed, 34.0)
        np.testing.assert_allclose(
            upper["vertices"] - ground["vertices"],
            np.tile([0, 0, 3], (8, 1)),
            atol=1e-14,
        )
        np.testing.assert_array_equal(upper["faces"], ground["faces"])
        with self.assertRaises(ValueError):
            expand(replace(leaf, width_m=1.30), fixed, 34.0)


if __name__ == "__main__":
    unittest.main()
