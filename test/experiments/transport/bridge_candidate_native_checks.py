"""Legacy actual MVT data cannot substitute missing retained candidate facts."""

import unittest
from pathlib import Path
from unittest.mock import patch

from canonical_port_native_inputs import FIXTURE, load_source
from outshine_ref.transport.canonical_ports._end_candidates import (
    inspect_bridge_end_candidates,
)


class ActualSourceWithoutRetainedCandidates(unittest.TestCase):
    def test_legacy_source_has_no_candidate_fallback_or_dem_work(self):
        original_read = Path.read_bytes
        accessed = []

        def source_only(path):
            accessed.append(path.name)
            if path.name == "dem.webp":
                raise AssertionError("missing candidate facts must not acquire DEM")
            return original_read(path)

        with (
            patch.object(Path, "read_bytes", source_only),
            patch(
                "canonical_port_native_inputs.decode_terrarium",
                side_effect=AssertionError("no DEM decode"),
            ) as decode,
            patch(
                "canonical_port_native_inputs.terrain_input",
                side_effect=AssertionError("no terrain setup"),
            ) as terrain,
        ):
            source, manifest = load_source(FIXTURE)
            self.assertEqual(len(source.nodes), manifest["native_capture"]["nodes"])
            self.assertEqual(
                sum(
                    len(span.attributes["native_directed_aliases"])
                    for span in source.spans
                ),
                manifest["native_capture"]["directed_edges"],
            )
            self.assertTrue(any(span.bridge for span in source.spans))
            original_nodes, original_spans, original_unknown = (
                source.nodes,
                source.spans,
                source.unknown_nodes,
            )
            result = inspect_bridge_end_candidates(
                source, (), (), source_binding=source.binding, candidate_binding={}
            )
            self.assertEqual(result["state"], "Pending")
            self.assertEqual(result["reason"], "source_or_candidate_binding_mismatch")
            self.assertEqual(result["groups"], ())
            self.assertEqual(result["qualified_contacts"], 0)
            self.assertFalse(any(result["work"].values()))
            self.assertIs(source.nodes, original_nodes)
            self.assertIs(source.spans, original_spans)
            self.assertIs(source.unknown_nodes, original_unknown)
            self.assertIs(result["source_binding"], source.binding)
            decode.assert_not_called()
            terrain.assert_not_called()
        self.assertEqual(accessed, ["native.json.gz", "recipes.json"])


if __name__ == "__main__":
    unittest.main()
