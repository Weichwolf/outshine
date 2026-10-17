"""Real scoped walls, preserved old owners, actual cavities and camera-stable plans."""

import copy
import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from elevation_mesh import compiled
from elevation_plan import plan
from elevation_sources import sources
from facade_entry_geometry import basis_for
from facade_parts import append
from provider_contract import attributes
from structural_shell_geometry import native
from tile_elevations import ElevationFacades
from tile_roof_crowns import CrownFacades
from verify_facade_frames import view_for


def build(owner_type, row, neighbors, view):
    owner = owner_type([row, *neighbors], view)
    source = owner.rows[0]
    mesh, info = owner(source, attributes(source["properties"]), 12)
    return mesh, info, owner


def untimed(value):
    if isinstance(value, dict):
        return {k: untimed(v) for k, v in value.items() if not k.endswith("_ms")}
    if isinstance(value, list):
        return [untimed(v) for v in value]
    return value


class ElevationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = sources()
        cls.original = copy.deepcopy(cls.rows)
        cls.before = [build(CrownFacades, row, n, view_for(row)) for row, n in cls.rows]
        cls.after = [
            build(ElevationFacades, row, n, view_for(row)) for row, n in cls.rows
        ]

    def test_actual_scope_partitions_safe_storeys_stable_ids_and_unchanged_sources(
        self,
    ):
        kinds = set()
        for (row, _neighbors), (_mesh, info, owner) in zip(
            self.rows, self.after, strict=True
        ):
            self.assertFalse(owner.rejected)
            self.assertEqual(owner.rows[0]["properties"], row["properties"])
            plans = owner.elevations.get(row["id"], ())
            for p in plans:
                kinds.add(p.kind)
                scopes = p.scopes
                self.assertEqual(scopes[0].base_m, 0)
                self.assertEqual(
                    scopes[-1].top_m,
                    info.get("eave_m", attributes(row["properties"])["buildingHeight"]),
                )
                for a, b in zip(scopes, scopes[1:]):
                    self.assertEqual(a.top_m, b.base_m)
                for s in scopes:
                    self.assertGreaterEqual((s.top_m - s.base_m) / s.rows, 2.64)
                regions = [shapely.box(0, s.base_m, p.width_m, s.top_m) for s in scopes]
                self.assertAlmostEqual(
                    shapely.union_all(regions).area,
                    p.width_m * (scopes[-1].top_m - scopes[0].base_m),
                    delta=1e-8,
                )
                self.assertEqual(len({f["id"] for f in p.frames()}), 3)
                self.assertTrue(
                    all(f["source_facade_id"] == p.facade_id for f in p.frames())
                )
            self.assertLessEqual(
                sum(f["opening_count"] for f in info.get("facade_plan", ())), 4096
            )
        self.assertEqual(kinds, {"vertical-punched", "horizontal-ribbons"})
        self.assertEqual(self.rows, self.original)

    def test_preserved_entry_shop_projection_fields_and_single_entry_planning(self):
        for before, after in zip(self.before, self.after, strict=True):
            _mesh, old, previous = before
            _mesh, new, owner = after
            for key in (
                "entrance_plan",
                "shopfront_plan",
                "balcony_plan",
                "window_bay_plan",
                "facade_programme",
            ):
                self.assertEqual(untimed(old.get(key)), untimed(new.get(key)), key)
            self.assertEqual(
                len(owner.entrances.planned), len(previous.entrances.planned)
            )
            entry = new.get("entrance_plan")
            if entry:
                original = entry["facade_id"]
                self.assertNotIn(
                    original,
                    {p.facade_id for p in owner.elevations.get(entry["source_id"], ())},
                )
            self.assertFalse(owner.recess_rejected)

    def test_complete_scoped_wall_hulls_match_independent_old_solid_and_metric_uvs(
        self,
    ):
        for index, ((row, neighbors), (_near, _info, new_owner)) in enumerate(
            zip(self.rows[:4], self.after[:4], strict=True)
        ):
            for factor in (1, 10000):
                view = view_for(row)
                view = replace(
                    view, eye=view.target + (view.eye - view.target) * factor
                )
                owner = CrownFacades([row, *neighbors], view)
                source = owner.rows[0]
                parameters = attributes(source["properties"])
                raw, info = owner.base(source, parameters, 12)
                from tile_facades import recipe_for

                bound, frames, _identifier = owner.surface(
                    raw,
                    source,
                    parameters | info,
                    recipe_for(source, parameters)["parameters"],
                    12,
                )
                plans = new_owner.elevations[row["id"]]
                changed, scopes = compiled(bound, frames, plans, owner.binding, 12)
                if row["id"] in owner.base.assets:
                    skin = getattr(
                        owner.base.assets[row["id"]],
                        info["roof_crown_product"] + "_skin",
                    )
                    original_solid = native(append(bound, [skin]))
                    actual = native(append(changed, [skin]))
                else:
                    original_solid, actual = native(bound), native(changed)
                self.assertAlmostEqual(
                    original_solid.volume(), actual.volume(), delta=1e-7
                )
                self.assertAlmostEqual(
                    original_solid.surface_area(), actual.surface_area(), delta=1e-7
                )
                self.assertLess(
                    (original_solid - actual).volume()
                    + (actual - original_solid).volume(),
                    1e-7,
                )
                from tile_facade_recess import selection

                xyz = changed["vertices"][changed["faces"]]
                normals = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
                normals /= np.linalg.norm(normals, axis=1, keepdims=True)
                for frame in scopes:
                    if "architecture_scope" not in frame:
                        continue
                    mask, _basis, rect = selection(xyz, normals, frame, frame["base_m"])
                    self.assertTrue(rect)
                    uv = changed["uv"][changed["faces"][mask]] * [
                        frame["width_m"] / frame["columns"],
                        frame["height_m"] / frame["rows"],
                    ]
                    for a, b in ((0, 1), (1, 2), (2, 0)):
                        np.testing.assert_allclose(
                            np.linalg.norm(uv[:, a] - uv[:, b], axis=1),
                            np.linalg.norm(xyz[mask, a] - xyz[mask, b], axis=1),
                            atol=1e-8,
                            rtol=0,
                        )

    def test_new_near_windows_have_actual_open_fronts_not_only_shader_masks(self):
        tested = 0
        for _mesh, info, owner in self.after:
            active = {r["id"] for r in owner.recesses}
            xyz = _mesh["vertices"][_mesh["faces"]]
            normal = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
            normal /= np.linalg.norm(normal, axis=1, keepdims=True)
            for f in info.get("facade_plan", ()):
                if "architecture_scope" not in f or f["id"] not in active:
                    continue
                basis = basis_for(f)
                q = (xyz - np.r_[f["a"], f["base_m"]]) @ basis.T
                front = (abs(q[:, :, 2]).max(axis=1) < 1e-8) & (
                    normal @ basis[2] > 0.999999
                )
                opaque = shapely.union_all(shapely.polygons(q[front, :, :2]))
                cell, storey = f["width_m"] / f["columns"], f["height_m"] / f["rows"]
                for c, r in ((0, 0), (f["columns"] - 1, f["rows"] - 1)):
                    self.assertFalse(
                        opaque.covers(
                            shapely.Point((c + 0.5) * cell, (r + 0.5) * storey)
                        )
                    )
                tested += 1
        self.assertGreaterEqual(tested, 6)

    def test_source_plans_do_not_change_when_old_roof_uses_its_coarse_product(self):
        for (row, neighbors), (_mesh, near, _owner) in zip(
            self.rows[:4], self.after[:4], strict=True
        ):
            v = view_for(row)
            far = replace(v, eye=v.target + (v.eye - v.target) * 10000)
            _mesh, info, owner = build(ElevationFacades, row, neighbors, far)
            self.assertFalse(owner.rejected)
            self.assertEqual(
                info["facade_architecture_plan"], near["facade_architecture_plan"]
            )
            self.assertFalse(owner.recesses)

    def test_low_source_fallback_is_exact_previous_output_and_invalid_scopes_rejected(
        self,
    ):
        a, old, _ = self.before[-1]
        b, new, _ = self.after[-1]
        for key in a:
            np.testing.assert_array_equal(a[key], b[key])
        self.assertEqual(untimed(old), untimed(new))
        p = self.after[0][2].elevations[self.rows[0][0]["id"]][0]
        original = self.after[0][2].source_elevations[self.rows[0][0]["id"]][0]
        f = next(f for f in original if f["id"] == p.facade_id)
        for bad in (f | {"rows": 2}, f | {"width_m": 3}, f | {"height_m": 5}):
            with self.assertRaises(ValueError):
                plan(bad, 0, p.kind, 0)
        row, neighbors = self.rows[0]
        owner = CrownFacades([row, *neighbors], view_for(row))
        source = owner.rows[0]
        params = attributes(source["properties"])
        raw, info = owner.base(source, params, 12)
        from tile_facades import recipe_for

        mesh, frames, _id = owner.surface(
            raw, source, params | info, recipe_for(source, params)["parameters"], 12
        )
        with self.assertRaises(ValueError):
            compiled(mesh, frames, [p, p], owner.binding, 12)

    def test_later_facade_failure_does_not_advertise_unapplied_scopes(self):
        row, neighbors = self.rows[0]
        with patch.object(
            ElevationFacades,
            "attach_recesses",
            side_effect=ValueError("failed opening operation"),
        ):
            mesh, info, owner = build(ElevationFacades, row, neighbors, view_for(row))
        self.assertNotIn("facade_architecture_plan", info)
        self.assertNotIn(row["id"], owner.elevations)
        self.assertTrue(
            any(r["reason"] == "failed opening operation" for r in owner.rejected)
        )
        self.assertIn("not applied", owner.elevation_rejected[-1]["reason"])
        self.assertGreater(native(mesh).volume(), 0)


if __name__ == "__main__":
    unittest.main()
