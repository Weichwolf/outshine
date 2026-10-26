"""Real source axes, retained openings/access, context contacts and deferred work."""

import copy
import json
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from facade_entrances import EntranceDetails
from facade_frame_geometry import FrameDetails, demand, footprint, placed
from facade_frame_programme import choose, placement
from facade_parts import append
from opening_modules import template as window_module
from outshine_ref.buildings.facade_frames import frame_surface, region
from provider_contract import attributes
from stepped_source_probe import overview
from structural_shell_geometry import native
from tile_facades import FacadeDetails


def sources():
    data = json.loads(
        Path(__file__).with_name("data").joinpath("facade-frames.json").read_text()
    )

    def row(saved):
        return saved | {"polygon": shapely.from_wkb(saved["footprint_wkb"])}

    return [(row(c["source"]), [row(r) for r in c["neighbors"]]) for c in data["cases"]]


def view_for(row):
    a, b, c, d = row["polygon"].bounds
    points = np.array(
        [
            [x, y, z]
            for x in (a, c)
            for y in (b, d)
            for z in (0, attributes(row["properties"])["buildingHeight"])
        ]
    )
    return overview(points, [1.0, -1.25, 0.8])


def build(row, neighbors, view, *, enabled=True, bands=True):
    builder = FacadeDetails([row, *neighbors], view)
    options = []
    if not enabled:
        options.append(patch("facade_frame_geometry.choose", return_value=((), [])))
    if not bands:
        options.append(
            patch(
                "tile_facade_bands.BandDetails.__call__",
                side_effect=lambda mesh, *args, **kwargs: append(
                    mesh, kwargs.get("additions", ())
                ),
            )
        )
    from contextlib import ExitStack

    with ExitStack() as stack:
        for option in options:
            stack.enter_context(option)
        mesh, info = builder(row, attributes(row["properties"]), 12)
    return mesh, info, builder


class FacadeFrameProgrammes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = []
        cls.original = []
        for row, neighbors in sources():
            cls.original.append(copy.deepcopy((row, neighbors)))
            view = view_for(row)
            before = build(row, neighbors, view, enabled=False, bands=False)
            after = build(row, neighbors, view, bands=False)
            cls.cases.append((row, neighbors, view, before, after))

    def test_actual_consumer_retains_all_underlying_triangles_and_function_fields(self):
        for index, (row, neighbors, _view, before, after) in enumerate(self.cases):
            first, old, _ = before
            last, new, builder = after
            self.assertTrue(builder.frames.completed)
            for key in (
                "vertices",
                "faces",
                "normals",
                "uv",
                "materials",
                "facade_extent",
            ):
                np.testing.assert_array_equal(last[key][: len(first[key])], first[key])
            for key in old:
                if not key.endswith("_ms"):
                    self.assertEqual(new[key], old[key], key)
            self.assertEqual(
                {r["kind"] for r in builder.frames.completed},
                {
                    "paired-piers"
                    if new["facade_recipe"] == "residential"
                    else "storey-grid"
                },
            )
            primary = new["entrance_plan"]["facade_id"]
            self.assertTrue(
                all(r["facade_id"] != primary for r in new["facade_frame_plan"])
            )
            self.assertEqual((row, neighbors), self.original[index])
            self.assertEqual(row["polygon"].wkb_hex, row["footprint_wkb"])

    def test_actual_frame_apertures_leave_every_existing_upper_window_and_casing_free(
        self,
    ):
        for row, _neighbors, _view, _before, (_mesh, info, builder) in self.cases:
            facades = info["facade_plan"]
            by_id = {f["id"]: f for f in facades}
            for record in builder.frames.completed:
                facade = by_id[record["facade_id"]]
                plan = placement(facade, 0.0, info["facade_recipe"])
                geometry = region(plan.frame)
                margin_u = float(
                    np.linalg.norm(np.asarray(plan.origin_xy) - facade["a"])
                )
                width, height = (
                    facade["width_m"] / facade["columns"],
                    facade["height_m"] / facade["rows"],
                )
                raw = window_module(
                    width=width * facade["fractions"][0],
                    height=height * facade["fractions"][1],
                )
                module_points = raw["vertices"][:, [0, 2]]
                lo, hi = module_points.min(axis=0), module_points.max(axis=0)
                for c in range(facade["columns"]):
                    for r in range(1, facade["rows"]):
                        origin = np.array(
                            [
                                (c + 0.5) * width - margin_u,
                                (r + 0.5) * height
                                - height * facade["fractions"][1] / 2
                                - plan.base_m,
                            ]
                        )
                        opening = shapely.box(*(origin + lo), *(origin + hi))
                        self.assertLess(geometry.intersection(opening).area, 1e-8)
                full = placed(frame_surface(plan.frame), plan, 12)
                solid = native(full)
                self.assertAlmostEqual(
                    solid.volume(), geometry.area * plan.frame.depth_m, places=7
                )
                self.assertLess(footprint(plan).intersection(row["polygon"]).area, 1e-8)

    def test_far_and_lookaway_need_precedes_template_expansion_but_logical_plan_persists(
        self,
    ):
        row, neighbors, view, _before, (_mesh, info, _builder) = self.cases[0]
        far = replace(view, eye=view.target + (view.eye - view.target) * 1000)
        with patch(
            "facade_frame_geometry.frame_surface",
            side_effect=AssertionError("expanded"),
        ):
            _, distant, builder = build(row, neighbors, far)
        self.assertEqual(distant["facade_frame_plan"], info["facade_frame_plan"])
        self.assertTrue(all(r["mode"] == "deferred" for r in builder.frames.completed))
        plan = placement(info["facade_plan"][0], 0.0, info["facade_recipe"])
        away = replace(view, target=view.eye + (view.eye - view.target))
        self.assertFalse(demand(plan, away))

    def test_near_local_template_failure_keeps_complete_prior_facade_and_reason(self):
        row, neighbors, view, before, _after = self.cases[0]
        with patch(
            "facade_frame_geometry.frame_surface",
            side_effect=ValueError("injected frame failure"),
        ):
            mesh, info, builder = build(row, neighbors, view, bands=False)
        for k in mesh:
            np.testing.assert_array_equal(mesh[k], before[0][k])
        self.assertNotIn("facade_frame_plan", info)
        self.assertTrue(any("injected" in r["reason"] for r in builder.frames.rejected))

    def test_programme_capacity_primary_front_and_small_gap_counterexamples(self):
        row, _neighbors, _view, _before, (_mesh, info, _builder) = self.cases[1]
        facades, parameters = info["facade_plan"], attributes(row["properties"])
        primary = next(
            f for f in facades if f["id"] == info["entrance_plan"]["facade_id"]
        )
        entry = {"frame": primary}
        plans, rejected = choose(
            row, parameters, facades, info["facade_recipe"], entry, max_triangles=0
        )
        self.assertEqual(plans, ())
        self.assertTrue(rejected)
        self.assertEqual(choose(row, parameters, facades, "industrial", entry)[0], ())
        self.assertEqual(
            choose(
                row | {"clipped": True},
                parameters,
                facades,
                info["facade_recipe"],
                entry,
            )[0],
            (),
        )
        frame = next(f for f in facades if f["columns"] >= 4 and f["rows"] >= 3)
        with self.assertRaisesRegex(ValueError, "clearance"):
            placement(frame | {"fractions": [0.99, 0.99]}, 0.0, info["facade_recipe"])

    def test_neighbor_height_and_source_wing_are_actual_local_admission_conditions(
        self,
    ):
        row, _neighbors, view, before, _after = self.cases[1]
        _mesh, info, _ = before
        parameters = attributes(row["properties"])
        facades = info["facade_plan"]
        primary = next(
            f for f in facades if f["id"] == info["entrance_plan"]["facade_id"]
        )
        entry = {"frame": primary}
        plans, _ = choose(row, parameters, facades, info["facade_recipe"], entry)
        plan = plans[0]
        support = footprint(plan)
        top = plan.base_m + plan.frame.height_m
        neighbor = {
            "id": "counter:frame-neighbor",
            "polygon": support,
            "properties": {"render_height": top + 2, "render_min_height": 0},
        }
        for bottom, expected in ((0, False), (top + 1, True)):
            current = neighbor | {
                "properties": neighbor["properties"] | {"render_min_height": bottom}
            }
            rows = [row, current]
            owner = FrameDetails(
                EntranceDetails(rows, shapely.STRtree([r["polygon"] for r in rows]))
            )
            _, replacements = owner.products(
                row, parameters, facades, view, 12, info["facade_recipe"], entry
            )
            self.assertEqual(plan.facade_id in replacements, expected)
        wing = row | {"polygon": row["polygon"].union(support)}
        owner = FrameDetails(
            EntranceDetails([wing], shapely.STRtree([wing["polygon"]]))
        )
        _, replacements = owner.products(
            wing, parameters, facades, view, 12, info["facade_recipe"], entry
        )
        self.assertNotIn(plan.facade_id, replacements)
        self.assertTrue(any("another wing" in r["reason"] for r in owner.rejected))


if __name__ == "__main__":
    unittest.main()
