import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from outshine.buildings.building_envelope import BuildingVolume, building_neighbours
from outshine.buildings.entrances import STREET_STOREY_M, frontages
from outshine.buildings.facade import FacadeView, draw_facade, flat_bays
from outshine.buildings.facades import FacadeRepeat
from outshine.buildings.street_facade import draw_street_storey
from outshine.world.mesh import Meshes


class FacadeTest(unittest.TestCase):
    def test_parts_of_one_street_front_share_an_entrance(self):
        shapes = [
            shapely.orient_polygons(shapely.box(x, 0, x + 14, 12)) for x in (-14, 0)
        ]
        homes = [
            SimpleNamespace(
                polygon=p,
                bottom=0.5,
                top=11,
                foundation=SimpleNamespace(apron_m=6),
                passages=(),
                group=1,
            )
            for p in shapes
        ]
        neighbours = building_neighbours(
            shapes, [[BuildingVolume(p, -0.3, 11)] for p in shapes]
        )
        road = [
            (
                "transportation",
                shapely.LineString([(-24, -6), (24, -6)]),
                {"class": "path"},
            )
        ]
        ground = {
            "ground": (
                [(-24, -12, 0), (24, -12, 0), (24, 0, 0), (-24, 0, 0)],
                [[0, 1, 2], [0, 2, 3]],
            )
        }
        entries = frontages(homes, neighbours, road, shapely.Polygon(), ground)
        self.assertEqual(len(entries), 1)
        index, entry = next(iter(entries.items()))
        self.assertFalse(entry.low)
        # Separate outlines can have distinct doors along the same continuous street.
        homes[1].group = 2
        self.assertEqual(
            len(frontages(homes, neighbours, road, shapely.Polygon(), ground)), 2
        )
        a, b = np.asarray(shapes[index].exterior.coords)[entry.side : entry.side + 2]
        mesh = Meshes()
        draw_street_storey(
            mesh,
            a,
            b,
            0.5,
            0.5 + STREET_STOREY_M,
            "wall #c68d94",
            FacadeView((0, -10)),
            None,
            entry,
        )
        self.assertIn("street trim", mesh)
        self.assertIn("street base #c68d94", mesh)
        self.assertIn("entrance door", mesh)
        self.assertGreater(np.min(np.asarray(mesh["glass"][0])[:, 2]), 1.4)

    def test_flat_facade_fills_the_wall_once(self):
        repeat = FacadeRepeat(29, 12, 8, 4, 0.42, 0.55)
        areas = {}
        triangles = {}
        for material, points, indices in flat_bays(repeat):
            a, b, c = points[indices].transpose(1, 0, 2)
            areas[material] = np.cross(b - a, c - a)[:, 2].sum() / 2
            triangles[material] = len(indices)
        self.assertAlmostEqual(sum(areas.values()), 29 * 12)
        self.assertAlmostEqual(areas["glass"], 29 * 12 * 0.42 * 0.55)
        self.assertEqual(triangles["glass"], 2 * repeat.columns * repeat.rows)

    def test_long_wall_keeps_near_relief_and_distant_openings_in_the_same_grid(self):
        meshes = Meshes()
        draw_facade(
            meshes,
            np.array([0, 0]),
            np.array([220, 0]),
            0,
            12,
            FacadeView((8, -24), 700),
            grounded=True,
            bays=True,
        )
        points = np.asarray(meshes["glass"][0])
        near = points[points[:, 1] > 0]
        far = points[points[:, 1] == 0]
        self.assertGreater(len(near), 0)
        self.assertGreater(len(far), 0)
        self.assertLess(near[:, 0].max(), far[:, 0].min())
        self.assertLess(np.asarray(meshes["window sill"][0])[:, 0].max(), 85)
        for detail in (near, far):
            self.assertEqual(len(np.unique(np.round(detail[:, 2], 6))), 8)
        self.assertEqual(np.asarray(meshes["wall"][0])[:, 0].max(), 220)
