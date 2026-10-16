"""Actual roof holes, panes/frames, source envelope and functional hall controls."""

import copy
import unittest
from unittest.mock import patch

import manifold3d as manifold
import numpy as np
import shapely
from industrial_hall_details import products
from industrial_roof_choice import section_for
from industrial_rooflights import (
    DEPTH_M,
    FRAME_M,
    GLASS_INSET_M,
    GLASS_M,
    assemble,
    components,
    plan,
)
from outshine_ref.buildings.section_roofs import RoofSection
from provider_contract import attributes
from structural_shell_geometry import native
from tile_roof_envelopes import choose, roof_surface
from verify_industrial_halls import (
    construct,
    hall_route,
    loading_access,
    personnel_access,
    sources,
    view_for,
)


def generate(row, neighbors, *, opened=1, method="ring"):
    owner, prior, info, hall, *_ = construct(row, neighbors, opened=opened)
    section = section_for(row, choose(row, attributes(row["properties"])))
    fields = plan(hall, section)
    mesh, shell, body, added, _ = assemble(
        hall,
        owner.base.current_envelope,
        fields,
        1,
        opened,
        view_for(row),
        method=method,
    )
    return mesh, shell, body, added, fields, hall, prior, info


def world(item, x, y, z):
    return np.r_[np.array([x, y]) @ item["axes"] + item["origin_xy"], z]


def light_paths(shell, fields):
    solid = native(shell.mesh, shell.vertex_roots)
    for item in fields:
        a, low, b, high = item["bounds"]
        for u in (0.25, 0.5, 0.75):
            for v in (0.25, 0.5, 0.75):
                x, y = a * (1 - u) + b * u, low * (1 - v) + high * v
                start = world(item, x, y, item["ceiling_m"] - 0.10)
                end = world(item, x, y, item["roof_top_m"] + 0.20)
                if solid.ray_cast(start, end):
                    raise ValueError("Opaque hall roof blocks an owned lightwell")


def roof_coverage(mesh, row, fields):
    top = roof_surface(row, choose(row, attributes(row["properties"])))[0]
    expected = top["vertices"][top["faces"]]
    actual = mesh["vertices"][mesh["faces"][mesh["materials"] == 9]]
    projection = shapely.union_all(shapely.polygons(actual[:, :, :2]))
    holes = shapely.union_all([shapely.from_wkb(f["footprint_wkb"]) for f in fields])
    target = row["polygon"].difference(holes)
    if projection.symmetric_difference(target).area > 1e-6:
        raise ValueError(
            "Roof replacement changed its source domain outside owned holes"
        )
    for point in actual.reshape(-1, 3):
        matches = []
        for triangle in expected:
            polygon = shapely.Polygon(triangle[:, :2])
            if polygon.buffer(1e-7).covers(shapely.Point(point[:2])):
                normal = np.cross(triangle[1] - triangle[0], triangle[2] - triangle[0])
                matches.append(
                    abs((point - triangle[0]) @ normal) / np.linalg.norm(normal)
                )
        if not matches or min(matches) > 1e-7:
            raise ValueError("Remaining roof no longer lies on its original section")


class IndustrialRooflights(unittest.TestCase):
    def test_existing_actual_access_is_preserved_for_every_gate_state(self):
        for row, neighbors in sources()[:3]:
            for opened in (0, 0.5, 1):
                _mesh, shell, _body, added, _fields, hall, *_ = generate(
                    row, neighbors, opened=opened
                )
                prior, _ = products(hall, 1, opened, view_for(row))
                collision = manifold.Manifold.batch_boolean(
                    [
                        native(shell.mesh, shell.vertex_roots),
                        *map(native, prior),
                        *map(native, added),
                    ],
                    manifold.OpType.Add,
                )
                self.assertEqual(collision.status(), manifold.Error.NoError)
                loading_access(hall, collision, opened)
                personnel_access(hall, collision, 90)
                hall_route(hall, collision)

    def test_real_holes_reach_rooms_and_preserve_source_roof_elsewhere(self):
        for row, neighbors in sources()[:3]:
            original = copy.deepcopy(row)
            mesh, shell, _body, added, fields, hall, prior, info = generate(
                row, neighbors
            )
            self.assertGreater(len(fields), 0)
            self.assertLessEqual(len(fields), 64)
            self.assertEqual(row, original)
            self.assertEqual(hall["room_id"], info["room_ids"][0][0])
            self.assertAlmostEqual(
                mesh["vertices"][:, 2].max(), prior["vertices"][:, 2].max()
            )
            light_paths(shell, fields)
            roof_coverage(mesh, row, fields)
            collision = manifold.Manifold.batch_boolean(
                [native(shell.mesh, shell.vertex_roots), *map(native, added)],
                manifold.OpType.Add,
            )
            self.assertEqual(collision.status(), manifold.Error.NoError)

    def test_actual_glass_plane_thickness_normals_frames_and_clear_aperture(self):
        for row, neighbors in sources()[:3]:
            _mesh, shell, _body, added, fields, *_ = generate(row, neighbors)
            for item, frame, glass in zip(fields, added[::2], added[1::2], strict=True):
                a, low, b, high = item["bounds"]
                slope = item["slope"]
                pane, rim = native(glass), native(frame)
                self.assertEqual(pane.status(), manifold.Error.NoError)
                self.assertEqual(rim.status(), manifold.Error.NoError)
                area = (b - a - 2 * FRAME_M / np.hypot(1, slope)) * (
                    high - low - 2 * FRAME_M
                )
                self.assertAlmostEqual(pane.volume(), area * GLASS_M, places=6)
                self.assertAlmostEqual(
                    rim.volume(), ((b - a) * (high - low) - area) * DEPTH_M, places=6
                )
                x, y = (a + b) / 2, (low + high) / 2
                roof_z = item["intercept_m"] + slope * x
                start, end = (
                    world(item, x, y, item["ceiling_m"] - 0.1),
                    world(item, x, y, roof_z + 0.1),
                )
                hits = pane.ray_cast(start, end)
                self.assertEqual(len(hits), 2)
                np.testing.assert_allclose(
                    [h.position[2] for h in hits],
                    [roof_z - GLASS_INSET_M - GLASS_M, roof_z - GLASS_INSET_M],
                    atol=1e-7,
                    rtol=0,
                )
                self.assertFalse(rim.ray_cast(start, end))
                expected = np.r_[-slope * np.asarray(item["axes"])[0], 1] / np.hypot(
                    1, slope
                )
                np.testing.assert_allclose(hits[-1].normal, expected, atol=1e-7)
                self.assertTrue(
                    native(shell.mesh, shell.vertex_roots).status()
                    == manifold.Error.NoError
                )

    def test_opaque_prior_is_detected_as_blocked_lightwell(self):
        row, neighbors = sources()[0]
        _owner, _prior, _info, hall, shell, *_ = construct(row, neighbors)
        fields = plan(
            hall, section_for(row, choose(row, attributes(row["properties"])))
        )
        with self.assertRaisesRegex(ValueError, "blocks an owned lightwell"):
            light_paths(shell, fields)

    def test_four_bars_and_ring_have_same_actual_frame_and_glass_volume(self):
        for row, neighbors in sources()[:3]:
            *_, fields, _hall, _prior, _info = generate(row, neighbors)
            ring = manifold.Manifold.batch_boolean(
                list(map(native, components(fields))), manifold.OpType.Add
            )
            bars = manifold.Manifold.batch_boolean(
                list(map(native, components(fields, method="bars"))),
                manifold.OpType.Add,
            )
            self.assertEqual(ring.status(), manifold.Error.NoError)
            self.assertEqual(bars.status(), manifold.Error.NoError)
            self.assertLess((ring - bars).volume() + (bars - ring).volume(), 1e-7)

    def test_rigid_frame_change_preserves_field_identity_and_metric_detail(self):
        row, neighbors = sources()[1]
        _owner, _mesh, _info, hall, *_ = construct(row, neighbors)
        section = section_for(row, choose(row, attributes(row["properties"])))
        before = plan(hall, section)
        angle = np.deg2rad(37)
        rotate = np.array(
            [[np.cos(angle), np.sin(angle)], [-np.sin(angle), np.cos(angle)]]
        )
        offset = np.array([12345.0, -23456.0])
        region = shapely.transform(
            shapely.from_wkb(hall["interior_wkb"]), lambda xy: xy @ rotate + offset
        )
        other = RoofSection(
            section.knots,
            tuple(np.asarray(section.axis_xy) @ rotate),
            tuple(np.asarray(section.origin_xy) @ rotate + offset),
        )
        after = plan(dict(hall, interior_wkb=region.wkb_hex), other)
        self.assertEqual([f["id"] for f in before], [f["id"] for f in after])
        for a, b in zip(before, after, strict=True):
            expected = shapely.transform(
                shapely.from_wkb(a["footprint_wkb"]), lambda xy: xy @ rotate + offset
            )
            self.assertLess(
                expected.symmetric_difference(
                    shapely.from_wkb(b["footprint_wkb"])
                ).area,
                1e-6,
            )
        for part in components(after):
            vertices, uv = part["vertices"][part["faces"]], part["uv"][part["faces"]]
            for first, last in ((0, 1), (1, 2), (2, 0)):
                np.testing.assert_allclose(
                    np.linalg.norm(vertices[:, first] - vertices[:, last], axis=1),
                    np.linalg.norm(uv[:, first] - uv[:, last], axis=1),
                    atol=1e-7,
                    rtol=0,
                )
            normals = np.cross(
                vertices[:, 1] - vertices[:, 0], vertices[:, 2] - vertices[:, 0]
            )
            normals /= np.linalg.norm(normals, axis=1, keepdims=True)
            np.testing.assert_allclose(
                normals, part["normals"][part["faces"][:, 0]], atol=1e-7
            )

    def test_planning_precedes_geometry_capacity_and_preserves_holes(self):
        hall = {
            "id": "control",
            "ceiling_m": 3.0,
            "interior_wkb": shapely.Polygon(
                [(0, 0), (40, 0), (40, 40), (0, 40)],
                [[(14, 10), (14, 30), (26, 30), (26, 10)]],
            ).wkb_hex,
        }
        section = RoofSection(
            ((0.0, 4.0), (10.0, 6.0), (20.0, 4.0), (30.0, 6.0), (40.0, 4.0))
        )
        with patch(
            "industrial_rooflights.components", side_effect=AssertionError("early mesh")
        ):
            fields = plan(hall, section)
            self.assertGreater(len(fields), 0)
            region = shapely.from_wkb(hall["interior_wkb"])
            for item in fields:
                self.assertTrue(region.covers(shapely.from_wkb(item["footprint_wkb"])))
            with self.assertRaisesRegex(ValueError, "capacity exceeded"):
                plan(hall, section, max_fields=1)
        with self.assertRaises(TypeError):
            fields[0]["id"] = "changed"
        for invalid in (0, 65, True, 1.2):
            with self.assertRaises(ValueError):
                plan(hall, section, max_fields=invalid)
        self.assertFalse(plan(hall, RoofSection(((0.0, 4.0), (40.0, 4.0)))))


if __name__ == "__main__":
    unittest.main()
