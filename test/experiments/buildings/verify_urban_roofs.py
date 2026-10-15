"""Urban hip geometry checked against independent offset sections and analytic volume."""

import copy
import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from provider_contract import attributes
from shapely.affinity import translate
from structural_shell_geometry import mesh_section
from tile_facades import project
from tile_roof_envelopes import choose
from verify_roof_envelopes import case


def sources():
    records = json.loads(
        Path(__file__).with_name("data").joinpath("urban-roofs.json").read_text()
    )
    return [
        row | {"polygon": shapely.from_wkb(row["footprint_wkb"]), "clipped": False}
        for row in records["cases"]
    ]


def control(polygon):
    return {
        "id": "analytic",
        "properties": {"render_height": 18.0},
        "polygon": polygon,
        "clipped": False,
    }


class UrbanRoofs(unittest.TestCase):
    def test_rectangle_volume_and_all_hip_slopes(self):
        row = control(shapely.box(0, 0, 20, 30))
        p = attributes(row["properties"])
        choice = choose(row, p)
        self.assertEqual(choice.kind, "hip")
        _, envelope, top, _ = case(row, p, choice, compare=True)
        height, slope = choice.rise_m, choice.rise_m / 10
        expected = 600 * choice.eave_m + 600 * height - 50 * height**2 / slope
        expected += 4 * height**3 / (3 * slope**2)
        self.assertAlmostEqual(envelope.volume_m3, expected, places=7)
        points = top["vertices"][top["faces"]]
        normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
        np.testing.assert_allclose(
            np.linalg.norm(normals[:, :2], axis=1) / normals[:, 2], slope, atol=1e-12
        )

    def test_concave_and_courtyard_sections_match_mitred_inward_wavefront(self):
        polygons = (
            shapely.Polygon([(0, 0), (30, 0), (30, 12), (15, 12), (15, 25), (0, 25)]),
            shapely.Polygon(
                [(0, 0), (40, 0), (40, 30), (0, 30)],
                [[(12, 8), (28, 8), (28, 22), (12, 22)]],
            ),
        )
        for polygon in polygons:
            row = control(polygon)
            p = attributes(row["properties"])
            choice = choose(row, p)
            _, envelope, top, _ = case(row, p, choice)
            points = top["vertices"][top["faces"]]
            n = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
            slopes = np.linalg.norm(n[:, :2], axis=1) / n[:, 2]
            np.testing.assert_allclose(slopes, slopes[0], atol=1e-9)
            for fraction in (0.1, 0.4, 0.8):
                z = choice.eave_m + fraction * choice.rise_m
                actual = mesh_section(envelope.mesh, envelope.vertex_roots, z)
                expected = polygon.buffer(
                    -fraction * choice.rise_m / slopes[0], join_style=2
                )
                self.assertLess(actual.symmetric_difference(expected).area, 1e-7)

    def test_real_sources_preserve_footprint_holes_top_and_attributes(self):
        for row in sources():
            original = copy.deepcopy(row)
            p = attributes(row["properties"])
            choice = choose(row, p)
            self.assertEqual(choice.kind, "hip")
            result, envelope, _, _ = case(row, p, choice)
            self.assertEqual(result["status"], "pass")
            self.assertEqual(
                float(envelope.mesh["vertices"][:, 2].max()), p["buildingHeight"]
            )
            self.assertEqual(row, original)
            self.assertEqual(choose(row | {"id": "different place/name"}, p), choice)

    def test_excluded_sources_and_explicit_roofs_keep_flat_alternative(self):
        row = control(shapely.box(0, 0, 20, 30))
        for props in (
            {"roof:shape": "flat"},
            {"roof_shape": "flat"},
            {"building": "industrial"},
            {"building": "church"},
            {"building:levels": 7},
            {"building:levels": "bad"},
            {"render_min_height": 1},
            {"hide_3d": True},
        ):
            changed = row | {"properties": row["properties"] | props}
            self.assertEqual(
                choose(changed, attributes(changed["properties"])).kind, "flat"
            )
        for polygon in (
            shapely.box(0, 0, 7, 60),
            shapely.box(0, 0, 60, 60),
            shapely.Point(0, 0).buffer(15),
        ):
            self.assertEqual(
                choose(row | {"polygon": polygon}, attributes(row["properties"])).kind,
                "flat",
            )
        self.assertEqual(
            choose(row | {"clipped": True}, attributes(row["properties"])).kind, "flat"
        )

    def test_actual_facade_frames_end_at_reserved_eave_on_all_source_rings(self):
        for row in sources():
            p = attributes(row["properties"])
            choice = choose(row, p)
            _, envelope, _, _ = case(row, p, choice)
            mesh, selected, facades = project(
                envelope.mesh,
                row,
                p | {"eave_m": choice.eave_m},
                (0.48, 0.53),
                3.2,
                2.9,
            )
            np.testing.assert_array_equal(
                mesh["vertices"][mesh["faces"]],
                envelope.mesh["vertices"][envelope.mesh["faces"]],
            )
            self.assertTrue(selected.any())
            for facade in facades:
                self.assertEqual(facade["height_m"], choice.eave_m - p["minHeight"])

    def test_supplied_levels_and_translated_footprint(self):
        row = control(shapely.box(0, 0, 20, 30))
        row["properties"]["building:levels"] = 6
        row["polygon"] = translate(row["polygon"], 23000, -31000)
        p = attributes(row["properties"])
        choice = choose(row, p)
        self.assertGreaterEqual(choice.eave_m, 6 * 2.64)
        self.assertEqual(choice.kind, "hip")
        case(row, p, choice)


if __name__ == "__main__":
    unittest.main()
