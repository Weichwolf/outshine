import unittest
from dataclasses import replace

import numpy as np
import shapely
from shapely.geometry import Point, box

from outshine_ref import (
    BuildingFootprint, FootprintAdjustment, HeightInterval, Hint, LocalFrame,
    Mesh, Reservation, SourceFeature, SourceInputs, buildings, prepare, transport, vegetation,
)
from outshine_ref.geometry import conform_triangles
from outshine_ref.pipeline import adjusted_footprints


class SpatialPipeline(unittest.TestCase):
    def setUp(self):
        self.frame = LocalFrame("fixture-enu")
        self.footprint = BuildingFootprint("house-1", box(0, 0, 4, 4), HeightInterval(0, 4))

    def test_complete_handoff_retains_sources_and_uses_adjusted_spaces(self):
        inputs = SourceInputs(self.frame, buildings=(self.footprint,))
        road = Reservation("road-1", box(3, 0, 5, 4), HeightInterval(0, 2), "vehicle-clearance")
        order, selected = [], []

        def roads(source):
            order.append("transport")
            self.assertIs(source, inputs)
            changed = BuildingFootprint("house-1", self.footprint.geometry.difference(road.geometry), self.footprint.heights)
            return transport.Plan(self.frame, (FootprintAdjustment(self.footprint, (changed,), "road-space"),), (road,))

        def houses(context):
            order.append("buildings")
            self.assertEqual(context.footprints[0].geometry.area, 12)
            self.assertEqual(context.sources.buildings[0].geometry.area, 16)
            self.assertEqual(context.reservations, (road,))
            occupied = Reservation("house-1", context.footprints[0].geometry, self.footprint.heights, "building")
            return buildings.Plan(self.frame, (occupied,), (Hint("house-1", "planting", Point(6, 1)),))

        def plants(context):
            order.append("vegetation")
            self.assertEqual(len(context.hints), 1)
            for point in (Point(1, 1), Point(4, 1), Point(6, 1)):
                if not any(space.geometry.covers(point) for space in context.reservations):
                    selected.append(tuple(point.coords[0]))
            return vegetation.Plan(self.frame)

        world = prepare(inputs, roads, houses, plants)
        self.assertEqual(order, ["transport", "buildings", "vegetation"])
        self.assertEqual(selected, [(6.0, 1.0)])
        self.assertIs(world.sources, inputs)
        self.assertTrue(self.footprint.geometry.equals(box(0, 0, 4, 4)))

    def test_bridge_and_boundary_contact_do_not_imply_volume_overlap(self):
        bridge = Reservation("bridge", box(0, 0, 4, 4), HeightInterval(8, 10), "traffic")
        contact = Reservation("contact", box(4, 0, 5, 4), HeightInterval(0, 2), "traffic")
        road = Reservation("road", box(3, 0, 5, 4), HeightInterval(0, 2), "traffic")
        self.assertFalse(bridge.overlaps(self.footprint))
        self.assertFalse(contact.overlaps(self.footprint))
        self.assertTrue(road.overlaps(self.footprint))

    def test_changed_source_duplicate_change_and_colliding_parts_are_rejected(self):
        replacement = BuildingFootprint("house-1", box(0, 0, 3, 4), self.footprint.heights)
        change = FootprintAdjustment(self.footprint, (replacement,), "road-space")
        with self.assertRaises(ValueError):
            adjusted_footprints((self.footprint,), (change, change))
        stale = BuildingFootprint("house-1", box(0, 0, 5, 4), self.footprint.heights)
        with self.assertRaises(ValueError):
            adjusted_footprints((stale,), (change,))
        foreign = BuildingFootprint("other-house", box(0, 0, 3, 4), self.footprint.heights)
        with self.assertRaises(ValueError):
            FootprintAdjustment(self.footprint, (foreign,), "road-space")
        retained = BuildingFootprint("house-1", box(5, 0, 6, 4), self.footprint.heights, (1,))
        split = BuildingFootprint("house-1", box(0, 0, 3, 4), self.footprint.heights, (1,))
        with self.assertRaises(ValueError):
            adjusted_footprints((self.footprint, retained), (FootprintAdjustment(self.footprint, (split,), "split"),))

    def test_frame_mismatch_stops_before_the_next_stage(self):
        inputs = SourceInputs(self.frame)

        def forbidden(context):
            self.fail("the next stage must not receive a plan from a different frame")

        with self.assertRaises(ValueError):
            prepare(inputs, lambda _: transport.Plan(LocalFrame("another-region")), forbidden, forbidden)

    def test_normalized_attributes_own_nested_values(self):
        attributes = {"context": {"uses": ["home", "shop"]}}
        source = SourceFeature("source-1", Point(0, 0), attributes)
        attributes["context"]["uses"].append("changed")
        self.assertEqual(source.attributes["context"]["uses"], ("home", "shop"))
        with self.assertRaises(TypeError):
            source.attributes["new"] = "value"

    def test_footprint_changes_preserve_building_semantics(self):
        source = replace(self.footprint, attributes={"building": "house", "colour": "ff0000"})
        adjusted = replace(source, geometry=box(0, 0, 3, 4))
        output = adjusted_footprints((source,), (FootprintAdjustment(source, (adjusted,), "road-space"),))
        self.assertEqual(output[0].attributes, source.attributes)
        with self.assertRaises(ValueError):
            FootprintAdjustment(source, (replace(adjusted, attributes={}),), "road-space")


class GeometryBuffers(unittest.TestCase):
    def test_mesh_owns_buffers_and_rejects_invalid_indices(self):
        positions = np.array([[0., 0., 0.], [1, 0, 0], [0, 1, 0]])
        faces = np.array([[0, 1, 2]])
        mesh = Mesh(positions, faces)
        positions[0] = 9
        faces[0] = 0
        np.testing.assert_array_equal(mesh.vertices[0], [0, 0, 0])
        np.testing.assert_array_equal(mesh.triangles, [[0, 1, 2]])
        with self.assertRaises(ValueError):
            mesh.vertices[0] = 7
        for invalid in (np.array([[0., 1., 2.]]), np.array([[0, 1, 3]])):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                Mesh(mesh.vertices, invalid)

    def test_interfaces_preserve_area_coordinates_and_real_gaps(self):
        vertices = np.array([[0., 0.], [4, 0], [0, 4], [1, 0], [3, 0], [0, 2], [3, -2]])
        original = vertices.copy()
        faces = np.array([[0, 1, 2], [3, 6, 4]])
        result, audit = conform_triangles(vertices, faces)
        self.assertEqual(len(result), 5)
        self.assertEqual(audit["split_edges"], 2)
        before = shapely.union_all(shapely.polygons(vertices[faces]))
        after = shapely.union_all(shapely.polygons(vertices[result]))
        self.assertTrue(before.equals(after))
        np.testing.assert_array_equal(vertices, original)
        moved = vertices @ np.array([[0., -1], [1, 0]]) + [1e6, -7e5]
        np.testing.assert_array_equal(conform_triangles(moved, faces)[0], result)
        gap = vertices.copy()
        gap[[3, 4], 1] = -.0001
        self.assertEqual(conform_triangles(gap, faces)[1]["split_edges"], 1)

    def test_empty_and_invalid_interface_requests(self):
        empty = np.empty((0, 3), dtype=int)
        vertices = np.empty((0, 2))
        result, audit = conform_triangles(vertices, empty)
        self.assertEqual(result.shape, (0, 3))
        self.assertEqual(audit["added_triangles"], 0)
        with self.assertRaises(ValueError):
            conform_triangles(np.array([[np.nan, 0]]), np.array([[0, 0, 0]]))


if __name__ == "__main__":
    unittest.main()
