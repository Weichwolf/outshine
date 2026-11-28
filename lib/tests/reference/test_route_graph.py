import unittest

from outshine.transport.route_graph import route_graph
from outshine.transport.route_tiles import WORLD_CELLS

WEST = (14, 8942, 5683)
EAST = (14, 8943, 5683)
ROAD = {"class": "secondary"}
RAIL = {"class": "rail", "subclass": "rail"}


def way(identifier, points, tags):
    return {
        "id": identifier,
        "properties": tags,
        "geometry": {"type": "LineString", "coordinates": points},
    }


def layer(*features):
    return {"extent": 16, "features": list(features)}


class RouteGraphTest(unittest.TestCase):
    def test_a_directed_road_continues_across_a_quantized_tile_seam(self):
        reverse = ROAD | {"oneway": -1}
        west = way(72, [(4, 4), (14, 4), (20, 5)], reverse)
        east = way(72, [(-5, 4), (5, 5), (10, 5)], reverse)
        graph = route_graph({WEST: layer(west), EAST: layer(east)})
        self.assertEqual(graph.seam_pairs, ((0, 1),))
        self.assertEqual(graph.frontiers, ())
        a, b = graph.paths
        seam = a.nodes[-1]
        self.assertEqual(seam.point[0], EAST[1] * (WORLD_CELLS >> EAST[0]))
        self.assertEqual(seam, b.nodes[0])
        self.assertNotEqual(seam.point[1], a.nodes[-2].point[1])
        edges = set(graph.connections("motor"))
        self.assertIn((b.nodes[1], seam), edges)
        self.assertIn((seam, a.nodes[-2]), edges)
        self.assertNotIn((seam, b.nodes[1]), edges)
        self.assertIn((seam, b.nodes[1]), set(graph.connections("walk")))
        self.assertEqual(graph, route_graph({EAST: layer(east), WEST: layer(west)}))

    def test_unverified_or_ambiguous_seam_does_not_connect(self):
        west = way(72, [(4, 4), (20, 4)], ROAD)
        east = way(73, [(-4, 4), (6, 4)], ROAD)
        graph = route_graph({WEST: layer(west), EAST: layer(east)})
        self.assertEqual(graph.seam_pairs, ())
        self.assertEqual(len(graph.frontiers), 2)
        self.assertNotEqual(graph.paths[0].nodes[-1], graph.paths[1].nodes[0])
        parallel = way(72, [(-4, 5), (7, 5)], ROAD)
        same_way = east | {"id": 72}
        graph = route_graph({WEST: layer(west), EAST: layer(same_way, parallel)})
        self.assertEqual(graph.seam_pairs, ())
        self.assertEqual(len(graph.frontiers), 3)
        self.assertEqual(
            graph, route_graph({EAST: layer(parallel, same_way), WEST: layer(west)})
        )

    def test_at_grade_contact_and_layer_separation(self):
        road = way(1, [(2, 8), (8, 8), (14, 8)], ROAD)
        rail = way(2, [(8, 2), (8, 8), (8, 14)], RAIL)
        bridge = way(3, [(4, 5), (8, 8), (12, 11)], ROAD | {"brunnel": "bridge"})
        subway = way(4, [(9, 3), (8, 8), (7, 15)], RAIL | {"layer": -1})
        graph = route_graph({WEST: layer(road, rail, bridge, subway)})
        (contact,) = graph.contacts
        nodes = [path.nodes[1] for path in graph.paths]
        self.assertEqual(set(nodes), {contact.road, contact.rail, nodes[2], nodes[3]})
        self.assertEqual(len(set(nodes)), 4)
        self.assertEqual(len({n.point for n in nodes}), 1)
        without_vertex = way(2, [(8, 2), (8, 14)], RAIL)
        self.assertEqual(route_graph({WEST: layer(road, without_vertex)}).contacts, ())

    def test_access_and_construction_keep_source_evidence(self):
        tags = ROAD | {"access": "private", "foot": "yes", "horse": "no"}
        private = way(1, [(2, 8), (8, 8)], tags)
        unfinished = way(2, [(8, 8), (14, 8)], {"class": "secondary_construction"})
        graph = route_graph({WEST: layer(private, unfinished)})
        self.assertEqual(tuple(graph.connections("motor")), ())
        self.assertEqual(len(tuple(graph.connections("walk"))), 2)
        self.assertEqual(dict(graph.paths[0].attributes.tags), tags)


if __name__ == "__main__":
    unittest.main()
