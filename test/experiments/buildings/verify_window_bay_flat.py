"""Projection bounds and actual old/new distant consumers, with unchanged near access."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from blender_city_mesh import cell_sizes
from demand import View
from facade_entry_geometry import basis_for
from facade_window_bays import demand, flat_error
from tile_facade_recess import attach
from verify_window_bays import camera, prepared, sources
from window_bay_geometry import products


def panes(current, owner, mode):
    parts = products(current, owner, 1, mode)
    return np.concatenate(
        [p["vertices"][p["faces"][p["materials"] == 3]] for p in parts]
    ).reshape(-1, 3)


def project(points, view):
    q = (points - view.eye) @ view.basis.T
    if np.any(q[:, 2] <= 0):
        raise ValueError("Projection control crossed the eye plane")
    return q[:, :2] * view.focal_px / q[:, 2:3]


class FlatBayConsumer(unittest.TestCase):
    def test_actual_pane_projection_samples_stay_below_the_unchanged_directional_bound(
        self,
    ):
        for row in sources():
            *_, owner, plan = prepared(row)
            current = plan["cells"][0]
            a, b = panes(current, owner, "compact"), panes(current, owner, "flat")
            views = [
                replace(camera(current, d), movement_m=m)
                for d in (10, 500, 100000)
                for m in (0.0, 2.0)
            ]
            edge = camera(current, 300)
            views.append(
                replace(
                    edge,
                    fov_deg=140,
                    target=edge.target + basis_for(current["frame"])[0] * 600,
                    movement_m=2,
                )
            )
            opposite = camera(current, 500)
            views.append(
                replace(opposite, eye=opposite.target * 2 - opposite.eye, movement_m=2)
            )
            for view in views:
                bound = flat_error(current, view)
                movement = view.movement_m
                offsets = [np.zeros(3)]
                if movement:
                    diagonal = np.ones(3) * (movement / np.sqrt(3))
                    offsets.extend(
                        [
                            *(np.eye(3) * movement),
                            *(-np.eye(3) * movement),
                            diagonal,
                            -diagonal,
                        ]
                    )
                for offset in offsets:
                    moved = replace(
                        view,
                        eye=view.eye + offset,
                        target=view.target + offset,
                        movement_m=0,
                    )
                    for t in (0.25, 0.5, 0.75, 1.0):
                        q = a + (b - a) * t
                        actual = np.linalg.norm(
                            project(q, moved) - project(a, moved), axis=1
                        ).max()
                        self.assertLessEqual(actual, bound + 1e-9)

    def test_demand_has_real_wide_angle_fallback_inside_and_subpixel_counterexamples(
        self,
    ):
        for row in sources():
            *_, owner, plan = prepared(row)
            current = plan["cells"][0]
            self.assertEqual(demand(current, camera(current, 10)), "through")
            self.assertEqual(demand(current, camera(current, 500)), "flat")
            self.assertIsNone(demand(current, camera(current, 100000)))
            view = camera(current, 300)
            side = basis_for(current["frame"])[0]
            edge = replace(view, fov_deg=140, target=view.target + side * 600)
            self.assertGreater(flat_error(current, edge), 0.5)
            self.assertEqual(demand(current, edge), "compact")
            eye = np.r_[
                current["pocket"].centroid.coords[0], current["floor_z_m"] + 1.6
            ]
            self.assertEqual(demand(current, View(eye, eye + [1, 0, 0])), "through")
            self.assertFalse(owner.templates)

    def test_actual_far_consumer_saves40triangles_per_cell_and_keeps_metric_surface_fields(
        self,
    ):
        from verify_shopfronts import Shopfronts

        for row in sources():
            mesh, frames, p, entry, _, plan = prepared(row)
            view = camera(plan["cells"][0], 500)
            kwargs = {"plain_wall_material": 1, "entrance": entry, "bays": plan}
            with patch("facade_window_bays.flat_error", return_value=float("inf")):
                before, old, _ = attach(
                    mesh, row, p, frames, view, lambda: 32769, 32768, **kwargs
                )
            after, records, _ = attach(
                mesh, row, p, frames, view, lambda: 32769, 32768, **kwargs
            )
            cells = [r for r in records if r.get("kind") == "window_bay"]
            self.assertEqual({r["mode"] for r in cells}, {"flat"})
            self.assertEqual(
                {r["id"] for r in old if r.get("kind") == "window_bay"},
                {r["id"] for r in cells},
            )
            self.assertEqual(
                len(before["faces"]) - len(after["faces"]), 40 * len(cells)
            )
            cell_sizes(after, [32768, 32769])
            excluded = {plan["frame"]["id"]: {tuple(r["cell"]) for r in cells}}
            Shopfronts.assert_upper_surface(
                self, before, after, frames, entry["floor_m"], excluded_cells=excluded
            )
            Shopfronts.assert_upper_surface(
                self, before, after, frames, 0, ceiling=entry["floor_m"]
            )
            # The first actual product covers the omitted rear with the unchanged opaque cell.
            current = plan["cells"][0]
            cover = products(current, plan["owner"], 1, "flat")[0]
            local = (
                cover["vertices"] - np.r_[current["centre"], current["base_m"]]
            ) @ basis_for(current["frame"]).T
            np.testing.assert_allclose(local[:, 2], 0, atol=1e-12)
            self.assertEqual(set(cover["materials"]), {1})

    def test_actual_near_consumer_remains_array_exact_when_flat_approximation_is_disabled(
        self,
    ):
        row = sources()[0]
        mesh, frames, p, entry, _, plan = prepared(row)
        kwargs = {"plain_wall_material": 1, "entrance": entry, "bays": plan}
        view = camera(plan["cells"][0], 10)
        after, records, _ = attach(
            mesh, row, p, frames, view, lambda: 32769, 32768, **kwargs
        )
        with patch("facade_window_bays.flat_error", return_value=float("inf")):
            before, old, _ = attach(
                mesh, row, p, frames, view, lambda: 32769, 32768, **kwargs
            )
        for key in after:
            np.testing.assert_array_equal(before[key], after[key])
        self.assertEqual(records, old)


if __name__ == "__main__":
    unittest.main()
