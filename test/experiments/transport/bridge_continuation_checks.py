"""Independent coverage/contact/normal controls and real pinned tile continuations."""

import hashlib
import json
import unittest

import numpy as np
import shapely
from bridge_continuation_experiment import inputs, usage
from bridge_continuation_recipe import TOP_ROLES, build
from capture_bridge_continuations import FIXTURE, PIN, connected_bridges, source_role
from outshine_ref.geometry import ProjectedSurfaceBudget, SurfaceSampler
from outshine_ref.transport import close_surface
from retained_surface import flat_domain, stitch, surface_domain
from shared_bridge_geometry import corridor
from surface_solid_checks import audit_closed


class ContinuationChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if hashlib.sha256(FIXTURE.read_bytes()).hexdigest() != PIN:
            raise ValueError("changed pinned continuation fixture")
        cls.captured, cls.approaches, cls.root, cls.old, cls.old_used = inputs()
        cls.original = json.dumps(cls.captured, sort_keys=True)
        cls.results = {
            m: build(
                cls.captured["sources"],
                cls.approaches,
                cls.root,
                cls.old,
                geometry_method=m,
            )
            for m in ("rebuild", "retain")
        }

    def test_compatible_positive_overlap_handles_distinct_clipping_endpoints(self):
        rows = []
        shapes = []
        for identity, xy, props in (
            (
                "tile0:same",
                [[0, 0], [2, 0]],
                {"subclass": "subway", "brunnel": "bridge", "layer": 1},
            ),
            (
                "tile1:same",
                [[1.9, 0], [4, 0]],
                {"subclass": "subway", "brunnel": "bridge", "layer": 1},
            ),
            (
                "other-layer",
                [[3, 0], [5, 0]],
                {"subclass": "subway", "brunnel": "bridge", "layer": 2},
            ),
            (
                "other-class",
                [[3, 0], [5, 0]],
                {"subclass": "footway", "brunnel": "bridge", "layer": 1},
            ),
        ):
            rows.append(({}, {"source_id": identity, "properties": props}))
            shapes.append(shapely.LineString(xy).buffer(0.1, cap_style="flat"))
        self.assertEqual(connected_bridges(rows, shapes, {"tile0:same"}), {0, 1})
        with self.assertRaisesRegex(ValueError, "source budget"):
            connected_bridges(rows, shapes, {"tile0:same"}, max_bridges=1)

    def test_unknown_levels_and_physical_recipes_remain_explicit(self):
        for props in (
            {"indoor": 1},
            {"brunnel": "tunnel"},
            {"brunnel": "bridge"},
            {"subclass": "steps"},
            {"subclass": "subway"},
            {"layer": 1},
            {"layer": True},
        ):
            role, reason = source_role(
                {"source_id": "unknown", "properties": props}, set()
            )
            self.assertIn(role, ("deferred", "unresolved"))
            self.assertTrue(reason)
        self.assertEqual(
            source_role({"source_id": "known", "properties": {"indoor": 1}}, {"known"})[
                1
            ],
            "buildings_floor_surface_not_planned",
        )

    def test_budget_end_keeps_the_previous_independently_valid_plan(self):
        for budget in (
            ProjectedSurfaceBudget(time_limit_s=0),
            ProjectedSurfaceBudget(max_faces=1),
            ProjectedSurfaceBudget(max_stations=2),
            ProjectedSurfaceBudget(max_interface_candidates=0),
        ):
            meshes, note = build(
                self.captured["sources"],
                self.approaches,
                self.root,
                self.old,
                budget=budget,
            )
            self.assertFalse(note["valid"])
            self.assertTrue(note["valid_partial_base"])
            self.assertIs(meshes, self.old)
            self.assertIn("budget", note["reason"])
            rows = usage(self.captured, note, self.old_used)
            self.assertEqual(
                {r["source_id"] for r in rows if r["status"] == "used"}, self.old_used
            )

    def test_duplicate_and_excessive_sources_do_not_destroy_the_base(self):
        original = self.captured["sources"]
        excessive = [dict(original[0], source_id=str(i)) for i in range(129)]
        for sources in (original + [original[0]], excessive):
            meshes, note = build(sources, self.approaches, self.root, self.old)
            self.assertFalse(note["valid"])
            self.assertIs(meshes, self.old)

    def test_both_real_methods_preserve_complete_domains_and_holes(self):
        for method, (meshes, note) in self.results.items():
            self.assertTrue(note["valid"], (method, note["reason"]))
            for roles, mesh, depth in (
                (TOP_ROLES, meshes[0], 0.35),
                (("ground",), meshes[1], 0.25),
            ):
                expected = shapely.union_all(
                    [
                        corridor(s)
                        for s in self.captured["sources"]
                        if s["role"] in roles
                    ]
                )
                actual = surface_domain(mesh)
                self.assertLessEqual(
                    actual.symmetric_difference(expected).area,
                    max(1e-7, expected.area * 1e-10),
                )
                self.assertLessEqual(actual.hausdorff_distance(expected), 1e-8)
                self.assertAlmostEqual(
                    audit_closed(close_surface(mesh, depth)),
                    expected.area * depth,
                    places=5,
                )
            self.assertGreater(len(meshes[0].triangles), len(self.old[0].triangles))

    def test_previously_chosen_affine_surfaces_keep_their_contacts(self):
        barycentric = np.array(
            [
                [1, 0, 0],
                [0, 1, 0],
                [0, 0, 1],
                [0.5, 0.5, 0],
                [0.5, 0, 0.5],
                [0, 0.5, 0.5],
                [1 / 3] * 3,
            ]
        )
        for meshes, note in self.results.values():
            for existing, result in zip(self.old, meshes):
                samples = np.einsum(
                    "pk,fkj->fpj", barycentric, existing.vertices[existing.triangles]
                ).reshape(-1, 3)
                hits = SurfaceSampler(result).sample(samples[:, :2])
                self.assertTrue(np.isfinite(hits.points_enu_m).all())
                self.assertLessEqual(
                    abs(hits.points_enu_m[:, 2] - samples[:, 2]).max(), 1e-7
                )
            self.assertLessEqual(
                note["ground"]["fit"]["maximum_height_change_m"], 2 + 1e-7
            )

    def test_whole_face_norms_clearance_and_profile_certificates(self):
        for meshes, note in self.results.values():
            self.assertGreaterEqual(
                note["clearance"]["minimum_clearance_m"], 4.5 - 1e-7
            )
            for mesh, grade in zip(meshes, (0.024, 0.025)):
                p = mesh.vertices[mesh.triangles]
                normals = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
                self.assertTrue(np.all(normals[:, 2] > 0))
                self.assertLessEqual(
                    (np.linalg.norm(normals[:, :2], axis=1) / normals[:, 2]).max(),
                    grade * (1 + 1e-7) + 1e-10,
                )
            self.assertTrue(note["top"]["each_face_within_one_height_chord"])
            self.assertLessEqual(
                note["top"]["whole_face_height_error_bound_m"], 0.010000101
            )
            east = note["profile"]["east"]
            self.assertLess(east["original_flat_clearance"]["minimum_clearance_m"], 4.5)
            self.assertTrue(east["c1_flat_endpoints"])
            self.assertGreater(east["deck_rise_m"], 2)

    def test_clipping_priors_are_not_ground_contacts_and_sources_remain_owned(self):
        self.assertEqual(json.dumps(self.captured, sort_keys=True), self.original)
        sources = self.captured["sources"]
        self.assertEqual(sum(s["role"] == "deck" for s in sources), 19)
        self.assertEqual(sum(s["role"] == "ground" for s in sources), 48)
        for meshes, note in self.results.values():
            rows = usage(self.captured, note, self.old_used)
            self.assertEqual(len(rows), 3259)
            self.assertEqual(sum(r["status"] == "used" for r in rows), 69)
            self.assertEqual(len(note["unresolved"]), 37)
            yard = next(s for s in sources if "1435426310:0" in s["source_id"])
            hit = SurfaceSampler(meshes[0]).sample(np.array(yard["xy"])[[0, -1]])
            self.assertGreater(
                (hit.points_enu_m[:, 2] - yard["end_heights_m"]).max(), 8
            )
        for source in sources:
            if source["role"] in ("deferred", "unresolved"):
                self.assertNotIn("end_heights_m", source)

    def test_compound_c1_profile_matches_independent_polynomial_face_samples(self):
        for meshes, note in self.results.values():
            profile = note["profile"]
            west, east = profile["west"], profile["east"]
            self.assertLess(
                east["transition_station_m"][1], west["transition_station_m"][0]
            )
            top = meshes[0]
            barycentric = np.array(
                [
                    [1, 0, 0],
                    [0, 1, 0],
                    [0, 0, 1],
                    [0.5, 0.5, 0],
                    [0.5, 0, 0.5],
                    [0, 0.5, 0.5],
                    [1 / 3] * 3,
                ]
            )
            points = np.einsum(
                "pk,fkj->fpj", barycentric, top.vertices[top.triangles]
            ).reshape(-1, 3)
            station = (points[:, :2] - west["origin_xy"]) @ west["direction_xy"]
            expected = np.full(len(station), float(self.root[0].vertices[0, 2]))
            for curve, mask in (
                (east, station < east["transition_station_m"][1]),
                (west, station > west["transition_station_m"][0]),
            ):
                first, last = curve["transition_station_m"]
                breaks = np.array(curve["curve_breaks_m"])
                coefficients = np.array(curve["curve_coefficients"])
                local = np.clip(station[mask] - first, 0, last - first)
                segments = np.minimum(
                    np.searchsorted(breaks, local, side="right") - 1,
                    len(coefficients) - 1,
                )
                x = local - breaks[segments]
                a, b, c, d = coefficients[segments].T
                expected[mask] = a + x * (b + x * (c + x * d))
                end = breaks[-1] - breaks[-2]
                self.assertAlmostEqual(float(coefficients[0, 1]), 0, places=10)
                self.assertAlmostEqual(
                    float(
                        coefficients[-1, 1]
                        + 2 * coefficients[-1, 2] * end
                        + 3 * coefficients[-1, 3] * end**2
                    ),
                    0,
                    places=10,
                )
            self.assertLessEqual(
                abs(points[:, 2] - expected).max(),
                note["top"]["whole_face_height_error_bound_m"],
            )

    def test_retained_geometry_preserves_translated_hole_and_disconnected_patch(self):
        shift = np.array([1e6, -2e6])
        a = shapely.box(0, 0, 10, 10).difference(shapely.box(3, 3, 7, 7))
        domain = shapely.union_all(
            [a, shapely.box(10, 2, 15, 8), shapely.box(20, 0, 22, 2)]
        )
        a = shapely.set_coordinates(a, shapely.get_coordinates(a) + shift)
        domain = shapely.set_coordinates(
            domain, shapely.get_coordinates(domain) + shift
        )
        old = flat_domain(a)
        extra = flat_domain(domain.difference(a))
        result, report = stitch(old, extra, domain)
        self.assertTrue(report["original_vertices_unchanged"])
        self.assertEqual(len(shapely.get_parts(surface_domain(result))), 2)
        self.assertEqual(
            sum(len(p.interiors) for p in shapely.get_parts(surface_domain(result))), 1
        )
        self.assertLessEqual(report["boundary_hausdorff_m"], 1e-8)
        with self.assertRaisesRegex(ValueError, "budget"):
            stitch(old, extra, domain, budget=ProjectedSurfaceBudget(max_vertices=3))


if __name__ == "__main__":
    unittest.main()
