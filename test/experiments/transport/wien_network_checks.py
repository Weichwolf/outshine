"""Whole field creases, explicit floor semantics, source accounting and traffic unions."""

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from affine_field_surface import mesh_field
from camera_height_inputs import digest
from ground_surface import ground_layout
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.transport import close_surface
from retained_surface import surface_domain
from surface_solid_checks import audit_closed
from transport_flat_plan import examples, plan
from transport_network import transport_widths
from wien_network_experiment import compare_assembly
from wien_network_sources import full_ledger, select
from wien_network_union import union_traffic


class NetworkGeometry(unittest.TestCase):
    def setUp(self):
        self.field = Mesh(
            [[0, 0, 0], [10, 0, 0], [10, 10, 0.1], [0, 10, 0]], [[0, 1, 2], [0, 2, 3]]
        )
        self.domain = shapely.box(0, 0, 10, 10)

    def test_height_crease_is_split_before_lifting_and_whole_body_closes(self):
        traffic = shapely.box(2, 1, 8, 9)
        mesh, road, report = mesh_field(self.field, traffic, self.domain)
        self.assertLessEqual(report["maximum_normalized_grade"], 1 + 1e-7)
        xyz = mesh.vertices[mesh.triangles]
        side = xyz[..., 0] - xyz[..., 1]
        self.assertTrue(
            np.all((side.min(axis=1) >= -1e-8) | (side.max(axis=1) <= 1e-8))
        )
        self.assertEqual(
            surface_domain(Mesh(mesh.vertices, mesh.triangles[road]))
            .symmetric_difference(traffic)
            .area,
            0,
        )
        self.assertAlmostEqual(audit_closed(close_surface(mesh, 0.25)), 25)
        np.testing.assert_allclose(
            mesh.vertices[:, 2],
            np.minimum(mesh.vertices[:, 0], mesh.vertices[:, 1]) * 0.01,
            atol=1e-12,
        )

    def test_invalid_or_overbudget_field_never_becomes_an_asset(self):
        with self.assertRaises(ValueError):
            mesh_field(self.field, shapely.box(2, 1, 8, 9), self.domain, max_faces=1)
        with self.assertRaises(ValueError):
            mesh_field(self.field, shapely.box(8, 1, 18, 9), self.domain)

    def test_cell_and_domain_union_keep_old_footprint_with_overlap(self):
        second = Mesh(self.field.vertices + [5, 0, 0], self.field.triangles)
        expected = self.domain.union(shapely.box(5, 0, 15, 10))
        for method in ("cells", "domain"):
            mesh, report = union_traffic(self.field, second, method)
            self.assertEqual(
                surface_domain(mesh).symmetric_difference(expected).area, 0
            )
            self.assertEqual(report["old_missing_m2"], 0)
        with self.assertRaises(ValueError):
            union_traffic(self.field, second, "domain", max_faces=1)

    def test_disconnected_source_border_islands_remain_valid_ground(self):
        domain = shapely.MultiPolygon([self.domain, shapely.box(20, 20, 21, 21)])
        layout, _proof = ground_layout(
            self.field.vertices[:, :2], self.field.triangles, domain
        )
        mesh = Mesh(np.c_[layout["xy"], np.zeros(len(layout["xy"]))], layout["faces"])
        result = SurfaceSampler(mesh).sample(np.array([[20.5, 20.5], [15.0, 15.0]]))
        self.assertEqual(result.reasons, ("", "outside_surface"))
        self.assertAlmostEqual(
            audit_closed(close_surface(mesh, 0.25)), domain.area * 0.25
        )

    def test_rejected_comparators_are_reported_without_blocking_the_selected_path(self):
        with (
            tempfile.TemporaryDirectory() as directory,
            patch(
                "wien_network_experiment.union_traffic",
                side_effect=ValueError("face_budget"),
            ),
        ):
            reports, dense = compare_assembly(
                self.field,
                self.domain,
                (self.field, self.field),
                self.field,
                None,
                Path(directory),
            )
        self.assertTrue(all(not r["valid"] for r in reports.values()))
        self.assertFalse(dense["fit"]["valid"])
        self.assertEqual(dense["fit"]["reason"], "no valid dense traffic layout")


class NetworkSources(unittest.TestCase):
    def test_nonzero_or_ambiguous_floor_is_not_a_ground_surface(self):
        recipes = json.loads(
            Path(__file__).with_name("transport_network_recipes.json").read_text()
        )
        for level in (1, -1, "1", "0;1"):
            roads = examples()["T"]
            roads[0]["properties"] = dict(roads[0]["properties"], level=level)
            _accepted, _graphs, result = plan(roads, recipes, transport_widths())
            self.assertEqual(result["usage"][0]["reason"], "deferred_floor_level")

    def test_width_inference_precedes_portal_guard_and_keeps_known_decisions(self):
        rows = [
            {
                "source_id": "infer-width",
                "coordinates": [[-1, 0], [1, 0]],
                "properties": {"class": "minor"},
                "width_m": None,
            },
            {
                "source_id": "old-floor",
                "coordinates": [[20, 0], [21, 0]],
                "properties": {"class": "path"},
                "width_m": 1.5,
            },
        ]
        old = [
            {
                "source_id": "infer-width",
                "status": "not_used",
                "reason": "outside_connected_transport_field",
            },
            {
                "source_id": "old-floor",
                "status": "not_used",
                "reason": "buildings_floor_surface_not_planned",
            },
        ]
        raw = {
            "origin_lat_lon": [0, 0],
            "roads": rows,
            "original_axes": rows,
            "receipts": [],
            "deferred_sources": [],
        }
        upper = Mesh([[10, 10, 5], [11, 10, 5], [10, 11, 5]], [[0, 1, 2]])
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source.json"
            source.write_text(json.dumps(raw))
            with (
                patch("wien_network_sources.INPUT_PIN", digest(source)),
                patch("wien_network_sources.source_usage", return_value=old),
            ):
                selected, original = select(
                    source,
                    {"origin_lat_lon": [0, 0]},
                    {},
                    {"open_trench": shapely.box(-0.1, -0.1, 0.1, 0.1)},
                    (upper, upper),
                )
        self.assertEqual(selected["roads"], [])
        ledger = full_ledger(selected, original, [])
        self.assertEqual(
            [r["reason"] for r in ledger],
            ["open_portal_air_reservation", "buildings_floor_surface_not_planned"],
        )
        with self.assertRaises(ValueError):
            full_ledger(dict(selected, roads=rows), original, [])


if __name__ == "__main__":
    unittest.main()
