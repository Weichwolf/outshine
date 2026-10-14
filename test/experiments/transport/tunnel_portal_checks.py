"""Real contacts, complete lining sections and budget/semantic counterexamples."""

import copy
import hashlib
import json
import unittest
from dataclasses import replace

import numpy as np
import shapely
from capture_tunnel_portals import FIXTURE, PIN, base_surfaces
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.transport import close_surface
from surface_solid_checks import audit_closed
from tunnel_portal_recipe import DEFAULT_BUDGET, HEADROOM_M, ROOF_M, build


class TunnelPortalChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if hashlib.sha256(FIXTURE.read_bytes()).hexdigest() != PIN:
            raise ValueError("pinned tunnel fixture changed")
        cls.fixture = json.loads(FIXTURE.read_text())
        cls.base, cls.origin = base_surfaces()
        cls.results = {
            method: build(cls.fixture, cls.base, method=method)
            for method in ("parabolic", "cubic")
        }

    def test_real_tunnel_methods_preserve_all_old_vertices_and_contacts(self):
        for method, (model, note) in self.results.items():
            self.assertTrue(note["valid"], (method, note["reason"]))
            self.assertEqual(note["preservation"]["maximum_height_difference_m"], 0.0)
            contact = SurfaceSampler(model["top"]).sample(self.base[0].vertices[:, :2])
            self.assertFalse(any(contact.reasons))
            np.testing.assert_allclose(
                contact.points_enu_m[:, 2],
                self.base[0].vertices[:, 2],
                atol=1e-7,
                rtol=0,
            )
            points = np.array([s["contact"]["xyz_m"] for s in self.fixture["sources"]])
            contact = SurfaceSampler(model["top"]).sample(points[:, :2])
            np.testing.assert_allclose(contact.points_enu_m, points, atol=1e-7, rtol=0)
            self.assertIs(model["ground"], self.base[1])

    def test_whole_roof_cover_headroom_and_bounded_grade(self):
        for model, note in self.results.values():
            self.assertGreaterEqual(note["cover"]["minimum_clearance_m"], 2.0 - 1e-7)
            self.assertGreaterEqual(
                note["headroom"]["minimum_clearance_m"], HEADROOM_M - 1e-7
            )
            self.assertLessEqual(
                note["floor"]["independent_maximum_gradient"], 0.024 + 1e-7
            )
            self.assertLessEqual(
                note["field"]["sampling"]["maximum_height_chord_bound_m"], 0.009 + 1e-10
            )
            self.assertFalse(note["field"]["far_clip_is_floor_contact"])
            self.assertGreater(model["open_trench"].area, 0.0)
            self.assertGreater(model["buried"].area, 0.0)

    def test_concrete_components_are_closed_without_blocking_far_air_sections(self):
        for model, note in self.results.values():
            walls = model["walls"]
            wall_domain = shapely.coverage_union_all(
                shapely.polygons(walls.vertices[walls.triangles, :2])
            )
            for source in self.fixture["sources"]:
                far = np.asarray(source["xy"])[
                    -1 if source["contact"]["endpoint_index"] == 0 else 0
                ]
                self.assertGreater(wall_domain.distance(shapely.Point(far)), 1.9)
            for surface, depth, offset in (
                (model["top"], 0.35, 0),
                (walls, HEADROOM_M, HEADROOM_M),
                (model["roof"], ROOF_M, 0),
            ):
                from outshine_ref.contracts import Mesh

                solid = close_surface(
                    Mesh(surface.vertices + [0, 0, offset], surface.triangles), depth
                )
                self.assertGreater(audit_closed(solid), 0.0)

    def assert_fallback(self, fixture=None, budget=DEFAULT_BUDGET):
        model, note = build(
            self.fixture if fixture is None else fixture, self.base, budget=budget
        )
        self.assertFalse(note["valid"])
        self.assertTrue(note["valid_partial_base"])
        self.assertTrue(note["reason"])
        self.assertEqual(note["used_source_ids"], [])
        self.assertIs(model["top"], self.base[0])
        self.assertIs(model["ground"], self.base[1])

    def test_zero_time_and_small_geometry_budgets_retain_valid_base(self):
        for budget in (
            replace(DEFAULT_BUDGET, time_limit_s=0),
            replace(DEFAULT_BUDGET, max_faces=1),
            replace(DEFAULT_BUDGET, max_stations=2),
            replace(DEFAULT_BUDGET, max_interface_candidates=0),
        ):
            self.assert_fallback(budget=budget)

    def test_tags_do_not_choose_height_or_indoor_floor(self):
        for key, value in (
            ("layer", True),
            ("layer", 0),
            ("brunnel", "bridge"),
            ("indoor", True),
        ):
            fixture = copy.deepcopy(self.fixture)
            fixture["sources"][0]["properties"][key] = value
            self.assert_fallback(fixture)

    def test_changed_contact_and_non_endpoint_contact_retain_base(self):
        for change in ("height", "xy", "index"):
            fixture = copy.deepcopy(self.fixture)
            contact = fixture["sources"][0]["contact"]
            if change == "height":
                contact["xyz_m"][2] += 1
            elif change == "xy":
                contact["xyz_m"][0] += 1
            else:
                contact["endpoint_index"] = 1
            self.assert_fallback(fixture)

    def test_duplicate_and_excess_sources_are_bounded(self):
        fixture = copy.deepcopy(self.fixture)
        fixture["sources"].append(fixture["sources"][0])
        self.assert_fallback(fixture)
        fixture["sources"] *= 4
        self.assert_fallback(fixture)

    def test_terrain_must_cover_every_roof_face(self):
        fixture = copy.deepcopy(self.fixture)
        fixture["terrain"]["vertices"] = [
            [x + 2000, y, z] for x, y, z in fixture["terrain"]["vertices"]
        ]
        self.assert_fallback(fixture)


if __name__ == "__main__":
    unittest.main()
