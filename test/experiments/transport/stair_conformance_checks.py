"""Actual western clipped bands: exact zero output faces and unchanged admission."""

import hashlib
import json
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from capture_stair_context import parts
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import conform_triangles
from outshine_ref.places import decode_vector
from outshine_ref.transport import HeightBudget, close_surface
from outshine_ref.transport.weighted_heights import fit_weighted_heights
from stair_context_inputs import FIXTURE as CONTEXT
from stair_cycle_inputs import sampler
from stair_junction_layout import INPUT, arrangement
from stair_network_geometry import vertex_links
from surface_height_checks import normal_gradients
from surface_solid_checks import audit_closed

FIXTURE = Path(__file__).with_name("fixtures") / "stair-west-ground-conformance.json"


def area_twice(xy, faces):
    points = xy[faces]
    a, b = points[:, 1] - points[:, 0], points[:, 2] - points[:, 0]
    return a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]


def original_sources(context):
    native = json.loads(INPUT.read_text())
    payload = INPUT.with_name(native["origin"]["payload_file"])
    receipt = dict(context["vector"], path=str(payload))
    sources = parts(
        decode_vector(receipt)["transportation"],
        tuple(receipt["tile"]),
        np.array(context["origin_lat_lon"]),
    )
    return payload, sources


class ActualClippedConformance(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case = json.loads(FIXTURE.read_text())
        cls.context = json.loads(CONTEXT.read_text())
        cls.payload, cls.sources = original_sources(cls.context)
        cls.rows = cls.case["rows"]
        for row in cls.rows:
            row["xy"] = np.array(row["xy"])
            row["original"] = {
                "bridge": False,
                "source": {
                    "feature": cls.sources[row["source_id"]]["original_feature"]
                },
            }
        cls.domains = [shapely.from_wkt(w) for w in cls.case["domains_wkt"]]
        cls.masks = [shapely.from_wkt(w) for w in cls.case["masks_wkt"]]
        recipe_path = FIXTURE.with_name(cls.case["source_fixture"])
        cls.sample = staticmethod(
            sampler(recipe_path, json.loads(recipe_path.read_text()), cls.context)
        )
        cls.raw = {}

        def capture(xy, faces, **limits):
            actual, note = conform_triangles(xy, faces, **limits)
            cls.raw.update(xy=xy.copy(), input=faces.copy(), output=actual.copy())
            return actual, note

        with patch("stair_junction_layout.conform_triangles", side_effect=capture):
            cls.xy, cls.faces, cls.owners, cls.note = arrangement(
                cls.rows, cls.domains, cls.masks
            )

    def test_prepared_axes_and_priors_are_original_in_the_same_source_frame(self):
        path = FIXTURE.with_name(self.case["source_fixture"])
        self.assertEqual(
            hashlib.sha256(path.read_bytes()).hexdigest(),
            self.case["source_fixture_sha256"],
        )
        self.assertEqual(
            hashlib.sha256(self.payload.read_bytes()).hexdigest(),
            self.case["source_payload_sha256"],
        )
        self.assertEqual([r["source_id"] for r in self.rows], self.case["source_ids"])
        for row in self.rows:
            actual = self.sources[row["source_id"]]
            np.testing.assert_array_equal(row["xy"], actual["xy_m"])
            np.testing.assert_array_equal(row["prior"], self.sample(row["xy"]))
            self.assertNotIn("brunnel", actual["properties"])

    def test_real_conformance_creates_two_exact_zero_faces_after_input_filter(self):
        self.assertTrue(np.all(area_twice(self.raw["xy"], self.raw["input"]) > 0))
        nonzero = area_twice(self.raw["xy"], self.raw["output"]) != 0
        self.assertEqual(len(nonzero), 205)
        self.assertEqual(int((~nonzero).sum()), 2)
        self.assertFalse(
            shapely.coverage_is_valid(shapely.polygons(self.xy[self.raw["output"]]))
        )
        np.testing.assert_array_equal(self.faces, self.raw["output"][nonzero])
        self.assertEqual(self.note["conformed_zero_faces_removed"], 2)

    def test_every_positive_output_face_and_whole_clipped_source_is_retained(self):
        self.assertTrue(np.all(area_twice(self.xy, self.faces) > 0))
        shapes = shapely.polygons(self.xy[self.faces])
        self.assertTrue(shapely.coverage_is_valid(shapes))
        for i, domain in enumerate(self.domains):
            actual = shapely.union_all(shapes[self.owners[:, i]])
            self.assertLess(actual.symmetric_difference(domain).area, 1e-7)

    def test_missing_positive_face_still_fails_the_original_area_gate(self):
        def broken(xy, faces, **limits):
            actual, note = conform_triangles(xy, faces, **limits)
            biggest = np.argmax(abs(area_twice(xy, actual)))
            return np.delete(actual, biggest, axis=0), note

        with (
            patch("stair_junction_layout.conform_triangles", side_effect=broken),
            self.assertRaisesRegex(ValueError, "changes source footprints"),
        ):
            arrangement(self.rows, self.domains, self.masks)

    def test_existing_weighted_kernel_closes_the_actual_double_and_float32_body(self):
        neighbour = self.case["frozen_neighbour"]
        xyz = np.array(neighbour["vertices_enu_m"])
        ground = shapely.union_all(shapely.polygons(xyz[neighbour["faces"], :2]))
        points = shapely.points(self.xy)
        fixed = shapely.dwithin(ground, points, 1e-7)
        for mask in self.masks:
            fixed |= shapely.dwithin(mask, points, 1e-8)
        limits = np.min(
            np.where(self.owners, [r["gradient"] for r in self.rows], np.inf), axis=1
        )
        frames = np.eye(2)[None] / (limits[:, None, None] - 0.0002)
        fit = fit_weighted_heights(
            Mesh(np.c_[self.xy, self.sample(self.xy)], self.faces),
            frames,
            reference_datum_m=self.case["reference_datum_m"],
            fixed_vertices=np.flatnonzero(fixed),
            budget=HeightBudget(max_vertices=4096, max_faces=8192),
        )
        self.assertIsNotNone(fit.mesh, str(fit.report))
        self.assertEqual(len(self.faces), 203)
        self.assertEqual(len(shapely.get_parts(shapely.union_all(self.domains))), 2)
        body = close_surface(fit.mesh, 0.3)
        for dtype in (np.float64, np.float32):
            actual = Mesh(body.vertices.astype(dtype), body.triangles)
            self.assertGreater(audit_closed(actual), 0)
            vertex_links(actual.vertices, actual.triangles)
            top = fit.mesh.vertices.astype(dtype).astype(float)
            slopes = np.linalg.norm(
                normal_gradients(top[:, :2], self.faces, top[:, 2]), axis=1
            )
            self.assertTrue(np.all(slopes <= limits))


if __name__ == "__main__":
    unittest.main()
