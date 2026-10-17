"""Actual facade apertures, closed skins, retained front signal and default compatibility."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from clad_facade_mesh import CladFacades, CladFrames, surface
from clad_facade_plan import placement
from facade_frame_geometry import placed
from opening_modules import template as window_module
from provider_contract import attributes
from test_facade_frames import closed
from test_opening_panels import hits
from verify_facade_frames import build as old_build
from verify_facade_frames import sources, view_for


def build(row, neighbors, view):
    owner = CladFacades([row, *neighbors], view)
    mesh, info = owner(row, attributes(row["properties"]), 12)
    return mesh, info, owner


class CladFacadesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = sources()
        cls.built = [
            build(row, neighbors, view_for(row)) for row, neighbors in cls.rows
        ]
        cls.plans = []
        for (row, _neighbors), (_mesh, info, owner) in zip(
            cls.rows, cls.built, strict=True
        ):
            facade = next(
                f
                for f in info["facade_plan"]
                if f["id"] == owner.frames.completed[0]["facade_id"]
            )
            for kind in ("broad-piers", "broad-spandrels"):
                cls.plans.append(
                    (facade, placement(facade, 0, info["facade_recipe"], kind))
                )

    def test_closed_oriented_metric_surfaces_and_actual_open_apertures(self):
        for facade, plan in self.plans:
            mesh = surface(plan, "closed")
            self.assertAlmostEqual(
                closed(self, mesh), plan.region.area * plan.depth_m, places=7
            )
            self.assertEqual(len(mesh["faces"]), plan.triangles("closed"))
            self.assertLess(plan.region.intersection(plan.protected).area, 1e-10)
            cell, floor = (
                facade["width_m"] / facade["columns"],
                facade["height_m"] / facade["rows"],
            )
            w, h = np.array(facade["fractions"]) * [cell, floor]
            module = window_module(width=w, height=h)["vertices"][:, [0, 2]]
            lo, hi = module.min(axis=0), module.max(axis=0)
            margin = np.linalg.norm(np.asarray(plan.origin_xy) - facade["a"])
            for col in range(facade["columns"]):
                for row in range(1, facade["rows"]):
                    centre = np.array(
                        [
                            (col + 0.5) * cell - margin,
                            (row + 0.5) * floor - h / 2 - plan.base_m,
                        ]
                    )
                    bounds = shapely.box(*(centre + lo), *(centre + hi))
                    self.assertLess(plan.region.intersection(bounds).area, 1e-10)
                    self.assertEqual(len(hits(mesh, centre + (lo + hi) / 2)), 0)

    def test_far_front_is_exact_closed_front_and_builds_no_side_or_back(self):
        for _facade, plan in self.plans:
            full, outer = surface(plan, "closed"), surface(plan, "outer")
            with patch(
                "clad_facade_mesh.sides", side_effect=AssertionError("sides built")
            ):
                cap = surface(plan, "front")
            for product in (full, outer):
                selected = np.all(
                    product["vertices"][product["faces"], 2] == plan.depth_m, axis=1
                )
                np.testing.assert_array_equal(
                    product["vertices"][product["faces"][selected]],
                    cap["vertices"][cap["faces"]],
                )
                np.testing.assert_array_equal(
                    product["normals"][product["faces"][selected]],
                    cap["normals"][cap["faces"]],
                )
            self.assertLess(len(cap["faces"]), len(outer["faces"]))
            self.assertTrue(np.all(cap["materials"] == 10))

    def test_two_families_differ_substantially_on_the_same_existing_axes(self):
        for i in (0, 2):
            a, b = self.plans[i][1], self.plans[i + 1][1]
            self.assertEqual(a.frame, b.frame)
            self.assertGreater(
                a.region.symmetric_difference(b.region).area, a.region.area * 0.3
            )
            self.assertEqual(a.protected.wkb, b.protected.wkb)

    def test_world_height_translation_preserves_local_programme(self):
        f, original = self.plans[0]
        moved = placement(f, 137, "residential", original.kind)
        self.assertLess(original.region.symmetric_difference(moved.region).area, 1e-9)
        self.assertAlmostEqual(moved.base_m - original.base_m, 137)
        mesh = placed(surface(moved, "closed"), moved, 10)
        self.assertGreaterEqual(mesh["vertices"][:, 2].min(), 137)

    def test_source_openings_entry_projections_and_recipe_fields_remain(self):
        for (row, neighbors), (_mesh, after, _owner) in zip(
            self.rows, self.built, strict=True
        ):
            _before_mesh, before, _builder = old_build(row, neighbors, view_for(row))
            for key in (
                "facade_plan",
                "facade_recipe",
                "entrance_plan",
                "shopfront_plan",
                "balcony_plan",
                "window_bay_plan",
            ):
                self.assertEqual(after.get(key), before.get(key), key)
            self.assertTrue(after["facade_frame_plan"])
            self.assertTrue(all("composition" in r for r in after["facade_frame_plan"]))

    def test_far_camera_keeps_plan_and_reuses_cached_front(self):
        for (row, _neighbors), (_mesh, info, owner) in zip(
            self.rows, self.built, strict=True
        ):
            view = view_for(row)
            far = replace(view, eye=view.target + 1000 * (view.eye - view.target))
            frame_owner = CladFrames(owner.entrances)
            args = (row, attributes(row["properties"]), info["facade_plan"])
            family = info["facade_recipe"]
            entry = owner.entrances.plan(
                row, attributes(row["properties"]), info["facade_plan"], family
            )
            with patch(
                "clad_facade_mesh.sides", side_effect=AssertionError("far sides built")
            ):
                parts, ids = frame_owner.products(*args, far, 12, family, entry)
            self.assertTrue(parts)
            self.assertTrue(all(r["mode"] == "front" for r in frame_owner.completed))
            records = frame_owner.logical_info(len(ids))
            with patch(
                "clad_facade_mesh.surface",
                side_effect=AssertionError("warm template rebuilt"),
            ):
                again, again_ids = frame_owner.products(*args, far, 12, family, entry)
            self.assertEqual(records, frame_owner.logical_info(len(again_ids)))
            for a, b in zip(parts, again, strict=True):
                for key in a:
                    np.testing.assert_array_equal(a[key], b[key])

    def test_invalid_requests_fail_before_surface_work(self):
        facade, plan = self.plans[0]
        for kind in ("unknown", None):
            with self.assertRaises(ValueError):
                placement(facade, 0, "residential", kind)
        with (
            patch("clad_facade_mesh.front", side_effect=AssertionError("built")),
            self.assertRaises(ValueError),
        ):
            surface(plan, "unknown")

    def test_known_height_contacts_reject_both_outer_and_front(self):
        row, _neighbors = self.rows[0]
        _mesh, info, owner = self.built[0]
        entry = owner.entrances.plan(
            row,
            attributes(row["properties"]),
            info["facade_plan"],
            info["facade_recipe"],
        )
        args = (
            row,
            attributes(row["properties"]),
            info["facade_plan"],
            view_for(row),
            12,
            info["facade_recipe"],
            entry,
        )
        frames = CladFrames(owner.entrances)
        with patch.object(owner.entrances, "clear", return_value=False):
            parts, ids = frames.products(*args)
        self.assertFalse(parts)
        self.assertFalse(ids)
        self.assertTrue(any("neighbor" in r["reason"] for r in frames.rejected))


if __name__ == "__main__":
    unittest.main()
