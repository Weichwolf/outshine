"""Independent closed-body audit and source-identity counterexamples."""

import json
from pathlib import Path
import unittest

import numpy as np

from outshine_ref.contracts import Mesh
from surface_solid_methods import close_surface, dictionary_boundary
from transport_flat_plan import examples, plan


def audit_closed(mesh):
    faces = mesh.triangles
    edges = faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    _, inverse, counts = np.unique(np.sort(edges, axis=1), axis=0, return_inverse=True, return_counts=True)
    signs = np.where(edges[:, 0] < edges[:, 1], 1, -1)
    sums = np.bincount(inverse, weights=signs, minlength=len(counts))
    if not np.all(counts == 2) or not np.all(sums == 0):
        raise ValueError("body is not a closed oriented indexed mesh")
    triangles = mesh.vertices[faces] - mesh.vertices[0]
    return float(np.einsum("ij,ij->i", triangles[:, 0], np.cross(triangles[:, 1], triangles[:, 2])).sum() / 6)



class SourceCases(unittest.TestCase):
    def test_supplied_ids_and_attributes_survive_duplicate_normalization(self):
        roads = examples()["T"]
        for i, road in enumerate(roads):
            road["source_id"] = f"source-{i}"
        roads.append(dict(roads[0], source_id="duplicate-source"))
        recipes = json.loads(Path(__file__).with_name("transport_network_recipes.json").read_text())
        original = [(r["line"].wkb, dict(r["properties"])) for r in roads]
        accepted, _, report = plan(roads, recipes, {})
        self.assertEqual([row["id"] for row in report["usage"]], ["source-0", "source-1", "duplicate-source"])
        self.assertEqual(report["states"]["duplicate_represented"], 1)
        self.assertEqual([r["source_id"] for r in accepted], ["source-0", "source-1"])
        self.assertEqual(original, [(r["line"].wkb, dict(r["properties"])) for r in roads])

    def test_invalid_supplied_id_is_rejected(self):
        roads = examples()["straight"]
        roads[0]["source_id"] = " "
        recipes = json.loads(Path(__file__).with_name("transport_network_recipes.json").read_text())
        with self.assertRaises(ValueError):
            plan(roads, recipes, {})

    def test_dictionary_reference_matches_complete_library_body(self):
        source = Mesh(np.array([[0., 0., 1.], [4, 0, 1], [4, 4, 1], [0, 4, 1]]), np.array([[0, 1, 2], [0, 2, 3]]))
        result = close_surface(source, .25)
        reference = close_surface(source, .25, dictionary_boundary)
        np.testing.assert_array_equal(result.vertices, reference.vertices)
        np.testing.assert_array_equal(result.triangles, reference.triangles)
        self.assertAlmostEqual(audit_closed(result), 4.)


if __name__ == "__main__":
    unittest.main()
