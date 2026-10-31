import unittest

from road_normal_adaptive_corpus import evaluate
from test_road_surface_lod import recipe


class AdaptiveCorpusTest(unittest.TestCase):
    def test_budget_exhaustion_retains_original_without_partial_mesh(self):
        row = recipe(height=0.01) | {"ordinal": 123, "form": "surface"}
        result = evaluate(
            (row, {"sampled_maximum_normal_degrees": 5.0}, 1, 65536, False)
        )
        self.assertEqual(result["status"], "budget_fallback")
        self.assertEqual(result["work"]["normal_queries"], 1)
        self.assertNotIn("triangles", result)

    def test_accepted_output_respects_diagnostic_and_position_limits(self):
        row = recipe(height=0.01) | {"ordinal": 123, "form": "surface"}
        result = evaluate(
            (row, {"sampled_maximum_normal_degrees": 5.0}, 512, 65536, False)
        )
        self.assertEqual(result["status"], "accepted")
        self.assertLessEqual(result["sampled_maximum_normal_degrees"], 4)
        self.assertLessEqual(result["position_bound_m"], 0.25)
        self.assertLessEqual(result["normal_queries"], 512)


if __name__ == "__main__":
    unittest.main()
