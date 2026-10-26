"""Road width/ports and low closed profiles with whole finite terrain contact."""

import unittest

import numpy as np
import shapely
from bridge_support_contacts import ContactSurface
from curb_geometry import audit
from curb_plan import HEIGHT_M, WIDTH_M, select, source_masks
from outshine_ref.contracts import Mesh
from outshine_ref.transport.supports import SupportVolumeBuilder


class Curbs(unittest.TestCase):
    def plane(self, z=0):
        return Mesh(
            [[-20, -20, z], [20, -20, z], [20, 20, z], [-20, 20, z]],
            [[0, 1, 2], [0, 2, 3]],
        )

    def domains(self):
        traffic = shapely.box(-10, -2, 10, 2)
        world = shapely.box(-20, -20, 20, 20)
        crossing = shapely.box(-1, -10, 1, 10)
        ports = shapely.union_all([shapely.Point(x, 0).buffer(2.7) for x in [-10, 10]])
        return traffic, world, crossing, ports

    def test_exterior_band_keeps_whole_road_and_crossing_and_end_ports(self):
        traffic, world, crossing, ports = self.domains()
        rows, excluded, _ = select(
            traffic,
            world,
            [traffic.buffer(2.2)],
            [["road"]],
            crossing,
            ports,
            shapely.Polygon(),
        )
        self.assertGreater(len(rows), 0)
        for row in rows:
            self.assertEqual(row["sources"], ["road"])
            self.assertLess(row["shape"].intersection(traffic).area, 1e-10)
            self.assertLess(row["shape"].intersection(excluded).area, 1e-10)
            self.assertLess(
                row["shape"].difference(traffic.buffer(WIDTH_M, quad_segs=4)).area,
                1e-10,
            )

    def test_actual_low_profile_is_closed_and_contacts_whole_ground(self):
        traffic, world, crossing, ports = self.domains()
        rows, excluded, _ = select(
            traffic,
            world,
            [traffic.buffer(2.2)],
            [["road"]],
            crossing,
            ports,
            shapely.Polygon(),
        )
        ground = self.plane()
        builder = SupportVolumeBuilder(ground, self.plane(HEIGHT_M))
        shape = next(r["shape"] for r in rows if not r["reason"])
        product, note = builder.build(shape, minimum_height_m=0.1199999)
        self.assertIsNotNone(product, note)
        proof = audit(product, shape, ContactSurface(ground), traffic, world, excluded)
        self.assertLess(proof["volume_error_m3"], 1e-10)

    def test_whole_interior_peak_cannot_be_hidden_by_matching_corners(self):
        low = Mesh([[0, 0, 0], [2, 0, 0], [2, 2, 0], [0, 2, 0]], [[0, 1, 2], [0, 2, 3]])
        upper = Mesh(low.vertices + [0, 0, HEIGHT_M], low.triangles)
        product, _ = SupportVolumeBuilder(low, upper).build(
            shapely.box(0, 0, 2, 2), minimum_height_m=0.1
        )
        peak = Mesh(
            [[0, 0, 0], [2, 0, 0], [2, 2, 0], [0, 2, 0], [1, 1, 0.05]],
            [[0, 1, 4], [1, 2, 4], [2, 3, 4], [3, 0, 4]],
        )
        with self.assertRaises(ValueError):
            audit(
                product,
                shapely.box(0, 0, 2, 2),
                ContactSurface(peak),
                shapely.box(-2, 0, 0, 2),
                shapely.box(-3, -3, 3, 3),
                shapely.Polygon(),
            )

    def test_only_explicit_used_paved_urban_sources_are_eligible(self):
        graph = {
            "lines": np.array([shapely.LineString([[0, 0], [5, 0]])], object),
            "half_widths": np.array([2.0]),
            "sources": [("road",)],
            "vertices": np.array([[0.0, 0], [5.0, 0]]),
            "edges": np.array([[0, 1]]),
        }
        raw = {"road": {"properties": {"class": "minor", "surface": "paved"}}}
        eligible, owners, _other, ports = source_masks([graph], raw, {"road"}, [])
        self.assertEqual(len(eligible), 1)
        self.assertEqual(owners, [["road"]])
        self.assertTrue(ports.covers(shapely.Point(0, 0)))
        raw["road"]["properties"]["class"] = "motorway"
        with self.assertRaises(ValueError):
            source_masks([graph], raw, {"road"}, [])


if __name__ == "__main__":
    unittest.main()
