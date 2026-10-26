"""Actual hosted uses, exact upper-window retention and independent display-front areas."""

import copy
import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from blender_city_mesh import cell_sizes
from demand import View
from facade_entrances import EntranceDetails
from facade_entry_geometry import basis_for
from facade_shopfronts import ShopfrontDetails
from tile_facade_recess import attach
from tile_shopfronts import shop_cells
from verify_recess_consumer import source_facades


def sources():
    saved = json.loads(
        Path(__file__).with_name("data").joinpath("shopfronts.json").read_text()
    )
    return [
        row | {"polygon": shapely.from_wkb(row["footprint_wkb"])}
        for row in saved["cases"]
    ]


def planned(row, neighbors=()):
    mesh, frames, parameters = source_facades(row)
    rows = [row, *neighbors]
    entrances = EntranceDetails(rows, shapely.STRtree([r["polygon"] for r in rows]))
    entry = entrances.plan(row, parameters, frames, "residential")
    if entry is None:
        raise AssertionError(entrances.rejected)
    shops = ShopfrontDetails(entrances)
    plan = shops.plan(row, entry)
    return mesh, frames, parameters, entry, plan, shops


def camera(plan, distance, opposite=False):
    entry, frame = plan["entry"], plan["frame"]
    basis = basis_for(frame)
    centre = np.mean(
        [(c["column"] + 0.5) * entry["cell_width_m"] for c in plan["cells"]]
    )
    target = np.r_[frame["a"], 1.6] + centre * basis[0]
    eye = target + distance * (basis[2] * (-1 if opposite else 1) + basis[0] * 0.35)
    return View(eye, target, width=1920, height=1080, fov_deg=48)


def consume(mesh, row, frames, parameters, entry, plan, distance, **kwargs):
    return attach(
        mesh,
        row,
        parameters,
        frames,
        camera(plan, distance),
        lambda: 32769,
        32768,
        plain_wall_material=1,
        entrance=entry,
        shopfront=plan,
        shop_material=lambda fractions: 32770,
        **kwargs,
    )


def upper_triangles(mesh, height):
    faces = mesh["faces"]
    selected = mesh["vertices"][faces, 2].min(axis=1) >= height
    values = np.concatenate(
        [
            mesh[k][faces[selected]].reshape(selected.sum(), -1)
            for k in ("vertices", "normals", "uv", "facade_extent")
        ]
        + [mesh["materials"][selected, None]],
        axis=1,
    )
    return sorted(tuple(row) for row in values)


class Shopfronts(unittest.TestCase):
    def assert_upper_surface(
        self, before, after, frames, floor, *, excluded_cells=None, ceiling=10000
    ):
        for frame in frames:
            basis, origin = basis_for(frame), np.r_[frame["a"], 0.0]
            scale = [
                frame["width_m"] / frame["columns"],
                frame["height_m"] / frame["rows"],
            ]
            domain = shapely.box(0, floor, frame["width_m"], ceiling).difference(
                shapely.union_all(
                    [
                        shapely.box(
                            c * scale[0],
                            r * scale[1],
                            (c + 1) * scale[0],
                            (r + 1) * scale[1],
                        )
                        for c, r in (excluded_cells or {}).get(frame["id"], ())
                    ]
                )
            )

            def fields(mesh, basis=basis, origin=origin, scale=scale, domain=domain):
                faces = mesh["faces"]
                points = mesh["vertices"][faces]
                local = (points - origin) @ basis.T
                n = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
                n /= np.linalg.norm(n, axis=1, keepdims=True)
                selected = (n @ basis[2] > 0.999999) & (
                    abs(local[:, :, 2]).max(axis=1) < 0.3
                )
                result = {}
                for index in np.flatnonzero(selected):
                    polygon = shapely.Polygon(local[index, :, :2]).intersection(domain)
                    if polygon.area <= 1e-12:
                        continue
                    material = int(mesh["materials"][index])
                    if material in (32768, 32769):
                        np.testing.assert_allclose(
                            mesh["uv"][faces[index]] * scale,
                            local[index, :, :2],
                            atol=1e-10,
                            rtol=0,
                        )
                    key = material, round(float(local[index, :, 2].mean()), 8)
                    result.setdefault(key, []).append(polygon)
                return {k: shapely.union_all(v) for k, v in result.items()}

            a, b = fields(before), fields(after)
            self.assertEqual(a.keys(), b.keys())
            for key in a:
                self.assertLess(a[key].symmetric_difference(b[key]).area, 1e-7)

    def test_actual_host_plan_is_state_and_camera_independent_with_upper_windows_exact(
        self,
    ):
        for row in sources():
            before = copy.deepcopy(row)
            mesh, frames, p, entry, plan, shops = planned(row)
            self.assertIsNotNone(plan, shops.rejected)
            self.assertNotIn(entry["column"], [c["column"] for c in plan["cells"]])
            for distance, expected in (
                (10, "recess"),
                (500, "compact"),
                (100000, None),
            ):
                out, records, rejected = consume(
                    mesh, row, frames, p, entry, plan, distance
                )
                emitted = [r for r in records if r.get("kind") == "shopfront"]
                self.assertEqual(
                    {r["mode"] for r in emitted},
                    {expected} if expected else set(),
                    rejected,
                )
                if expected:
                    self.assertEqual(
                        {r["id"] for r in emitted}, {c["id"] for c in plan["cells"]}
                    )
                    old, _, _ = attach(
                        mesh,
                        row,
                        p,
                        frames,
                        camera(plan, distance),
                        lambda: 32769,
                        32768,
                        plain_wall_material=1,
                        entrance=entry,
                    )
                    if distance == 10:
                        self.assertEqual(
                            upper_triangles(old, entry["floor_m"]),
                            upper_triangles(out, entry["floor_m"]),
                        )
                    self.assert_upper_surface(old, out, frames, entry["floor_m"])
                    self.assertTrue(
                        np.isfinite(cell_sizes(out, (32768, 32769, 32770))).all()
                    )
                else:
                    self.assertIs(out, mesh)
            self.assertEqual(row, before)

    def test_upper_underground_ambiguous_and_missing_host_never_become_ground_shops(
        self,
    ):
        original = sources()[0]
        for level, layer in ((1, 0), (-1, 0), ("0;1", 0), (False, 0), (0, 1)):
            row = copy.deepcopy(original)
            for e in row["context"]["evidence"]:
                if e.get("role") in ("retail", "food"):
                    e.update(level=level, vertical_layer=layer)
            self.assertIsNone(planned(row)[4])
        row = copy.deepcopy(original)
        for e in row["context"]["evidence"]:
            if e.get("role") in ("retail", "food"):
                e["position_xy"] = [1e8, 1e8]
        self.assertIsNone(planned(row)[4])
        row["context"]["evidence"] = []
        self.assertIsNone(planned(row)[4])

    def test_missing_level_is_explicit_assumption_and_source_ground_level_remains_distinct(
        self,
    ):
        a, b = planned(sources()[0]), planned(sources()[2])
        self.assertEqual(
            {r["level_origin"] for r in a[4]["cells"]}, {"supplied-ground-level"}
        )
        self.assertEqual(
            {r["level_origin"] for r in b[4]["cells"]}, {"procedural-ground-assumption"}
        )

    def test_actual_recess_glass_area_depth_and_door_cell_exclusion(self):
        for row in sources():
            _, _, _, entry, plan, _ = planned(row)
            parts, _ = shop_cells(
                plan, "recess", lambda fractions: 32770, 1, lambda: 32769
            )
            basis, origin = basis_for(plan["frame"]), np.r_[plan["frame"]["a"], 0.0]
            width, height = entry["cell_width_m"], entry["floor_m"]
            for part, cell in zip(parts, plan["cells"], strict=True):
                triangles = part["vertices"][part["faces"]]
                local = (triangles - origin) @ basis.T
                glass = part["materials"] == 32769
                area = (
                    np.linalg.norm(
                        np.cross(
                            triangles[:, 1] - triangles[:, 0],
                            triangles[:, 2] - triangles[:, 0],
                        ),
                        axis=1,
                    )
                    / 2
                )
                fw, fh = cell["fractions"]
                self.assertAlmostEqual(
                    area[glass].sum(),
                    (width * fw - 0.12) * (height * fh - 0.12),
                    places=9,
                )
                np.testing.assert_allclose(local[glass, :, 2], -0.18, atol=1e-10)
                self.assertGreaterEqual(
                    local[:, :, 0].min(), cell["column"] * width - 1e-10
                )
                self.assertLessEqual(
                    local[:, :, 0].max(), (cell["column"] + 1) * width + 1e-10
                )
                self.assertLessEqual(local[:, :, 1].max(), height + 1e-10)

    def test_neighbor_blocks_display_cells_and_zero_capacity_keeps_original_entrance(
        self,
    ):
        from outshine_ref.buildings.interiors.geometry import rectangle

        for row in sources():
            mesh, frames, p, entry, plan, _ = planned(row)
            width = entry["cell_width_m"]
            basis, a = basis_for(plan["frame"]), np.asarray(plan["frame"]["a"])
            blockers = [
                rectangle(
                    a + (c["column"] + 0.5) * width * basis[0, :2],
                    basis[0, :2],
                    width,
                    0.03,
                    -0.001,
                )
                for c in plan["cells"]
            ]
            other = {
                "id": "neighbor-negative",
                "properties": {"render_height": 10},
                "polygon": shapely.union_all(blockers),
            }
            entrances = EntranceDetails(
                [row, other], shapely.STRtree([row["polygon"], other["polygon"]])
            )
            self.assertIsNone(ShopfrontDetails(entrances).plan(row, entry))
            out, records, rejected = consume(
                mesh, row, frames, p, entry, plan, 10, max_cells=0
            )
            self.assertFalse(any(r.get("kind") == "shopfront" for r in records))
            self.assertTrue(any(r.get("kind") == "entrance" for r in records), rejected)
            self.assertEqual(out["vertices"][:, 2].max(), mesh["vertices"][:, 2].max())
