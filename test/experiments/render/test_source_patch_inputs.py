"""Actual whole-source binding and refusal of altered recorded inputs."""

import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from source_patch_inputs import read_bound_sources

DATA = (
    Path(__file__).resolve().parents[2] / "outshine/data/render/building-depth-samples"
)


class SourcePatchInputs(unittest.TestCase):
    def test_whole_actual_sources_keep_all_words_and_unknown_parts(self):
        source, context, provenance = read_bound_sources(DATA)
        self.assertEqual(provenance["native_operand_files_exact"], 18)
        self.assertEqual(provenance["prepared_source_ordinals"], 1558)
        self.assertEqual(len(source["indices"]), 38476)
        self.assertEqual(context["triangle_binding"].shape, (38476, 6))
        self.assertTrue(np.all(context["triangle_binding"][:, 2] == -1))
        self.assertIsNone(provenance["runtime_world_revision"])
        self.assertEqual(len(context["binding"]["parts"]), 3)

    def test_altered_frame_ordering_material_and_missing_source_are_refused(self):
        original = json.loads((DATA / "source-binding.json").read_text())
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.source-patch-inputs."
        ) as directory:
            path = Path(directory) / "binding.json"
            for mutation in ("frame", "ordering", "material", "cells"):
                binding = json.loads(json.dumps(original))
                if mutation == "frame":
                    binding["capture_frame"]["east"][0] += 1e-8
                elif mutation == "ordering":
                    binding["parts"][0]["triangle_binding"]["sha256"] = "0" * 64
                elif mutation == "material":
                    binding["parts"][0]["material"] = 2
                else:
                    binding["cells"] = [25, 33]
                path.write_text(json.dumps(binding))
                with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                    read_bound_sources(DATA, binding_path=path)


if __name__ == "__main__":
    unittest.main()
