"""Independent placement counterexamples, source scope and owned detailed templates."""

import unittest

import numpy as np
import shapely
from furniture_mesh import template
from outshine_ref.buildings.interiors import Furnishing, access, layout, placed
from room_programme import BASIN, DESK, MEETING, STORAGE, TEA, WC, office_scope
from shapely.affinity import rotate, translate
from verify_openings import closed_parts


class PlacementControls(unittest.TestCase):
    def test_concave_room_does_not_accept_four_inside_corners_over_a_notch(self):
        room = shapely.Polygon(
            [(0, 0), (6, 0), (6, 6), (4, 6), (4, 2), (2, 2), (2, 6), (0, 6)]
        )
        spec = Furnishing("wide", 5.0, 1.0, 1.0, 0.8)
        item = placed(spec, [3.0, 3.0], [1.0, 0.0], "notch")
        self.assertTrue(
            all(
                room.covers(shapely.Point(p))
                for p in list(item["footprint"].exterior.coords)[:-1]
            )
        )
        with self.assertRaises(ValueError):
            access(room, [(0.5, 0.5)], [item])

    def test_transformed_actual_placements_keep_containment_and_connected_targets(self):
        for angle in (0.0, 31.0, 95.0):
            room = translate(
                rotate(shapely.box(0, 0, 8, 6), angle, origin=(0, 0)), 112.0, -418.0
            )
            portal = translate(
                rotate(shapely.Point(0.5, 3.0), angle, origin=(0, 0)), 112.0, -418.0
            )
            result = layout(
                room,
                [next(iter(portal.coords))],
                (DESK, STORAGE),
                "rotated",
                method="jittered-anchors",
                beam_width=4,
            )
            self.assertEqual(result["status"], "complete")
            for item in result["items"]:
                self.assertTrue(room.covers(item["activity"]))
            for path in result["proof"]["paths"]:
                self.assertTrue(result["proof"]["free"].covers(path))

    def test_replay_limits_and_inputs(self):
        room = shapely.box(0, 0, 8, 6)
        args = (room, [(0.5, 3.0)], (DESK, STORAGE), "replay")
        a = layout(*args, method="jittered-anchors", beam_width=4)
        b = layout(*args, method="jittered-anchors", beam_width=4)
        self.assertEqual(a["status"], b["status"])
        self.assertEqual(a["tested"], b["tested"])
        for x, y in zip(a["items"], b["items"], strict=True):
            np.testing.assert_array_equal(x["centre"], y["centre"])
        for width in (0, 9, True):
            with self.assertRaises(ValueError):
                layout(*args, beam_width=width)
        with self.assertRaises(ValueError):
            Furnishing("bad", 1.0, float("nan"), 1.0, 1.0)

    def test_actual_closed_templates_have_declared_bounds_and_nonzero_volume(self):
        for spec in (DESK, MEETING, STORAGE, TEA, WC, BASIN):
            mesh = template(spec)
            self.assertGreater(closed_parts(mesh), 0.0)
            xyz = mesh["vertices"]
            np.testing.assert_allclose(
                xyz.max(0) - xyz.min(0),
                [spec.width_m, spec.depth_m, spec.height_m],
                atol=1e-9,
                rtol=0,
            )

    def test_office_poi_does_not_fabricate_unavailable_level_or_building_use(self):
        base = {
            "source_properties": {},
            "storeys": 1,
            "context": {
                "evidence": [
                    {
                        "id": "office-1",
                        "role": "office",
                        "properties": {"class": "office"},
                        "level": 1,
                    }
                ]
            },
        }
        floors, evidence = office_scope(base)
        self.assertEqual(floors, ())
        self.assertEqual(evidence["out_of_plan_level_ids"], ["office-1"])
        self.assertEqual(base["source_properties"], {})
        unknown = base | {"context": {"evidence": []}}
        self.assertEqual(office_scope(unknown)[0], ())
        supplied = base | {"source_properties": {"building": "office"}, "storeys": 3}
        self.assertEqual(office_scope(supplied)[0], (0, 1, 2))


if __name__ == "__main__":
    unittest.main()
