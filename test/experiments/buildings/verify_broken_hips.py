"""Analytic volume/slopes, actual source sections and unchanged compact fallbacks."""

import copy
import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from broken_hip import envelope, recipe, surface
from outshine_ref.buildings.modelling import heightfield_envelope
from provider_contract import attributes
from structural_shell_geometry import mesh_section
from stepped_source_probe import overview
from tile_roof_envelopes import OrdinaryDetails, choose, roof_surface
from verify_roof_envelopes import measures
from verify_urban_roofs import control, sources


def products(row):
    parameters = attributes(row["properties"])
    choice = choose(row, parameters)
    top = roof_surface(row, choice)[0]
    upper = surface(top, choice.eave_m, choice.rise_m)
    product = heightfield_envelope(upper, parameters["minHeight"])
    return parameters, choice, top, upper, product


class BrokenHips(unittest.TestCase):
    def test_rectangle_analytic_volume_slopes_normals_and_exact_break_section(self):
        p, choice, _top, upper, product = products(control(shapely.box(0, 0, 20, 30)))
        solid = measures(product)

        def integral(r):
            return 600 * r - 500 * r**2 + 400 / 3 * r**3

        roof_volume = choice.rise_m * (
            0.65 / 0.15 * integral(0.15) + 0.35 / 0.85 * (integral(1) - integral(0.15))
        )
        self.assertAlmostEqual(
            product.volume_m3, choice.eave_m * 600 + roof_volume, places=7
        )
        triangles = upper["vertices"][upper["faces"]]
        normal = np.cross(
            triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        )
        normal /= np.linalg.norm(normal, axis=1, keepdims=True)
        np.testing.assert_allclose(
            upper["normals"].reshape(-1, 3, 3), np.repeat(normal[:, None], 3, axis=1)
        )
        slopes = np.linalg.norm(normal[:, :2], axis=1) / normal[:, 2]
        expected = choice.rise_m / 10 * np.array([0.65 / 0.15, 0.35 / 0.85])
        self.assertLess(abs(slopes[:, None] - expected).min(axis=1).max(), 1e-12)
        self.assertEqual(
            float(product.mesh["vertices"][:, 2].max()), p["buildingHeight"]
        )
        # The old mesh_section helper excludes triangles wholly on either side
        # of a plane and misses an entire knot ring. Native section is independent.
        knot = choice.eave_m + choice.rise_m * 0.65
        section = solid.scale((1000,) * 3).slice(knot * 1000)
        self.assertAlmostEqual(section.area() / 1e6, (20 - 3) * (30 - 3), places=7)

    def test_actual_concave_and_courtyard_sections_keep_the_same_wavefront(self):
        polygons = (
            shapely.Polygon([(0, 0), (30, 0), (30, 12), (15, 12), (15, 25), (0, 25)]),
            shapely.Polygon(
                [(0, 0), (40, 0), (40, 30), (0, 30)],
                [[(12, 8), (28, 8), (28, 22), (12, 22)]],
            ),
        )
        for row in [*(control(p) for p in polygons), *sources()]:
            before = copy.deepcopy(row)
            p, choice, top, _, product = products(row)
            measures(product)
            previous = heightfield_envelope(top, p["minHeight"])
            region = shapely.from_wkb(product.footprint_wkb)
            self.assertLess(region.symmetric_difference(row["polygon"]).area, 1e-7)
            for f in (0.1, 0.64, 0.66, 0.9):
                source = (
                    0.15 * f / 0.65 if f < 0.65 else 0.15 + 0.85 * (f - 0.65) / 0.35
                )
                actual = mesh_section(
                    product.mesh,
                    product.vertex_roots,
                    choice.eave_m + choice.rise_m * f,
                )
                expected = mesh_section(
                    previous.mesh,
                    previous.vertex_roots,
                    choice.eave_m + choice.rise_m * source,
                )
                self.assertLess(actual.symmetric_difference(expected).area, 1e-7)
            self.assertEqual(
                float(product.mesh["vertices"][:, 2].max()), p["buildingHeight"]
            )
            self.assertEqual(row, before)

    def test_uv_and_face_fields_follow_actual_source_interpolation(self):
        row = control(shapely.box(0, 0, 20, 30))
        p = attributes(row["properties"])
        choice = choose(row, p)
        top = roof_surface(row, choice)[0]
        top["uv"] = top["vertices"][:, :2] * [2, 3] + [7, 11]
        top["materials"] = np.arange(len(top["faces"]), dtype=np.uint16)
        top["groups"] = top["materials"].astype(np.uint32) + 100
        top["parts"] = top["groups"] + 100
        snapshot = {k: v.copy() for k, v in top.items()}
        upper = surface(top, choice.eave_m, choice.rise_m)
        np.testing.assert_allclose(
            upper["uv"], upper["vertices"][:, :2] * [2, 3] + [7, 11], atol=1e-12
        )
        np.testing.assert_array_equal(upper["groups"], upper["materials"] + 100)
        np.testing.assert_array_equal(upper["parts"], upper["materials"] + 200)
        for k in top:
            np.testing.assert_array_equal(top[k], snapshot[k])

    def test_early_capacity_invalid_profile_and_channel_rejections(self):
        _, choice, top, _, _ = products(control(shapely.box(0, 0, 20, 30)))
        with patch("broken_hip.prepare_source", side_effect=AssertionError("expanded")):
            with self.assertRaisesRegex(ValueError, "capacity"):
                surface(top, choice.eave_m, choice.rise_m, max_triangles=1)
            for run, height in ((0, 0.65), (0.8, 0.5), (0.2, 1), (float("nan"), 0.5)):
                with self.assertRaises(ValueError):
                    surface(
                        top,
                        choice.eave_m,
                        choice.rise_m,
                        run_fraction=run,
                        height_fraction=height,
                    )
            with self.assertRaisesRegex(ValueError, "inheritance"):
                surface(top | {"uv1": top["uv"]}, choice.eave_m, choice.rise_m)
            plateau = dict(top, vertices=top["vertices"].copy())
            plateau["vertices"][:, 2] = choice.eave_m + choice.rise_m * 0.15
            with self.assertRaisesRegex(ValueError, "coplanar"):
                surface(plateau, choice.eave_m, choice.rise_m)

    def test_recipe_uses_morphology_not_source_identity_and_respects_supplied_fields(
        self,
    ):
        row = sources()[0]
        p = attributes(row["properties"])
        choice = choose(row, p)
        selected = recipe(row, p, choice)
        self.assertIsNotNone(selected)
        self.assertEqual(recipe(row | {"id": "another-city/name"}, p, choice), selected)
        for key in ("roof:shape", "roof_height", "roof:colour"):
            changed = row | {"properties": row["properties"] | {key: "supplied"}}
            self.assertIsNone(recipe(changed, p, choice))

    def test_local_profile_error_preserves_exact_previous_closed_hip(self):
        row = sources()[0]
        p = attributes(row["properties"])
        choice = choose(row, p)
        top = roof_surface(row, choice)[0]
        baseline = heightfield_envelope(
            top, p["minHeight"], wall_material=12, base_material=12
        )
        with patch(
            "broken_hip.surface", side_effect=ValueError("injected local control")
        ):
            product, report = envelope(top, p, 12, choice, recipe(row, p, choice))
        for k in baseline.mesh:
            np.testing.assert_array_equal(product.mesh[k], baseline.mesh[k])
        self.assertEqual(report["status"], "hip-fallback")
        self.assertIn("injected", report["reason"])

    def test_actual_tile_consumer_and_need_before_surface_construction(self):
        row = sources()[0]
        p = attributes(row["properties"])
        choice = choose(row, p)
        top = roof_surface(row, choice)[0]
        view = overview(top["vertices"], [1.0, -1.25, 0.8])
        builder = OrdinaryDetails([], view)
        mesh, info = builder.complete(row, p, 12, choice)
        self.assertEqual(info["roof_profile"]["status"], "constructed")
        self.assertEqual(float(mesh["vertices"][:, 2].max()), p["buildingHeight"])
        far = replace(view, eye=view.target + (view.eye - view.target) * 10000)
        builder = OrdinaryDetails([], far)
        with patch(
            "tile_roof_envelopes.roof_surface", side_effect=AssertionError("expanded")
        ):
            _, info = builder.complete(row, p, 12, choice)
        self.assertEqual(info["roof_profile"]["status"], "deferred")
        self.assertLess(info["projected_geometry_error_px"], 0.5)


if __name__ == "__main__":
    unittest.main()
