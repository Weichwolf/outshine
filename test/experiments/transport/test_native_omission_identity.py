"""Native omission identities survive the reader without changing source admission."""

import gzip
import json
import unittest

from canonical_port_native_inputs import FIXTURE, load_source, source_input


def read_omission(fragment, *, present=True):
    row = {"Source": 0, "Reason": 5, "Feature": [1] * 32}
    if present:
        row["Fragment"] = fragment
    snapshot = {
        "Nodes": [], "Paths": [], "References": [], "Edges": [], "Coverage": [],
        "Omissions": [row], "SourceIdentity": {"DatasetId": "fixture", "Revision": "1"},
        "Sources": [], "IdentitySpace": 1,
    }
    return source_input(snapshot, {}, {"native_capture": "fixture", "recipe_source_sha256": "0" * 64})


class NativeOmissionIdentity(unittest.TestCase):
    def test_actual_fragment_identity_is_retained(self):
        fragment = list(range(32))
        source = read_omission(fragment)
        self.assertEqual(source.binding["omissions"][0]["Fragment"], tuple(fragment))
        self.assertEqual(source.spans, ())

    def test_zero_identity_is_not_unknown(self):
        source = read_omission([0] * 32)
        self.assertEqual(source.binding["omissions"][0]["Fragment"], (0,) * 32)

    def test_feature_wide_and_historical_unknown_are_retained(self):
        self.assertIsNone(read_omission(None).binding["omissions"][0]["Fragment"])
        self.assertNotIn("Fragment", read_omission(None, present=False).binding["omissions"][0])

    def test_truncated_or_nonbyte_identity_is_rejected(self):
        for fragment in ([0] * 31, [0] * 33, [-1] * 32, [256] * 32,
                         [True] * 32, [0.0] * 32, "00" * 32):
            with self.subTest(fragment=fragment), self.assertRaisesRegex(ValueError, "32-byte"):
                read_omission(fragment)

    def test_fragment_identity_does_not_relax_existing_source_guard(self):
        original, manifest = load_source()
        with gzip.open(FIXTURE / "native.json.gz", "rt") as stream:
            snapshot = json.load(stream)
        recipes = json.loads((FIXTURE / "recipes.json").read_text())
        for omission in snapshot["Omissions"]:
            omission["Fragment"] = list(range(32))
        enriched = source_input(snapshot, recipes, manifest)
        self.assertEqual(len(enriched.nodes), 4433)
        self.assertEqual(len(enriched.spans), 4743)
        self.assertEqual(enriched.nodes, original.nodes)
        self.assertEqual(enriched.spans, original.spans)
        self.assertTrue(original.unknown_nodes)
        self.assertEqual(enriched.unknown_nodes, original.unknown_nodes)


if __name__ == "__main__":
    unittest.main()
