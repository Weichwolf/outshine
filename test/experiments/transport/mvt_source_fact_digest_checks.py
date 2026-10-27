"""Controlled native-field byte vectors; no native/MVT execution claim."""

import copy
import math
import unittest

from mvt_source_fact_digest import native_source_fact_digest


class NativeSourceFactDigest(unittest.TestCase):
    def setUp(self):
        self.revision = "a" * 64
        self.partition = bytes(range(32))
        self.payload = {
            "SourceGeneration": 7,
            "SourceWayCount": 11,
            "Groups": [
                {
                    "FirstMember": 0,
                    "MemberCount": 1,
                    "OriginalEndpointCount": 2,
                    "FilteredEndpointCount": 1,
                    "Complete": False,
                    "Frontier": True,
                }
            ],
            "Members": [
                {
                    "Edge": {
                        "PathId": 0x0102030405060708,
                        "SegmentOrdinal": 3,
                        "Direction": 1,
                    },
                    "DomainNode": 0x1112131415161718,
                    "NodeGroup": 0,
                    "StreetWay": 7,
                    "SourceStep": 4,
                    "End": 1,
                }
            ],
            "Ways": [
                {
                    "StreetWay": 7,
                    "Feature": 8,
                    "Ring": 9,
                    "PointCount": 6,
                    "HalfWidthWord": 0x3FE00000,
                    "MaximumGradientWord": 0x3DF5C28F,
                    "EndStencil": [
                        {"LongitudeDeg": lon, "LatitudeDeg": 48.0}
                        for lon in (16.0, 16.001, 16.002, 16.003)
                    ],
                    "OwnsEnds": [True, False],
                }
            ],
        }

    def digest(self, payload=None, **kwargs):
        return native_source_fact_digest(
            self.revision,
            self.partition,
            self.payload if payload is None else payload,
            **kwargs,
        )

    def test_complete_literal_byte_vector_with_independent_openssl_golden(self):
        self.assertEqual(
            self.digest().hex(),
            "7b77727a9ff9cd6098ac603b0902896d85d8f3f3bc9d8c9e399cb23977923ed9",
        )

    def test_empty_vector_has_its_own_scope_bound_golden(self):
        payload = dict(self.payload, Groups=[], Members=[], Ways=[])
        self.assertEqual(
            self.digest(payload).hex(),
            "d164e6c09ebe88437b71737252b0dab0f1f5eebdae5581d2286f9ad3ab740391",
        )

    def test_network_partition_generation_and_way_scope_are_bound(self):
        original = self.digest()
        self.assertNotEqual(
            original, native_source_fact_digest("b" * 64, self.partition, self.payload)
        )
        self.assertNotEqual(
            original, native_source_fact_digest(self.revision, bytes(32), self.payload)
        )
        for key in ("SourceGeneration", "SourceWayCount"):
            payload = copy.deepcopy(self.payload)
            payload[key] += 1
            self.assertNotEqual(original, self.digest(payload))

    def test_each_native_group_and_member_word_is_bound(self):
        original = self.digest()
        for key, value in self.payload["Groups"][0].items():
            payload = copy.deepcopy(self.payload)
            payload["Groups"][0][key] = (
                (not value) if type(value) is bool else value + 1
            )
            self.assertNotEqual(original, self.digest(payload), key)
        for key in ("DomainNode", "NodeGroup", "StreetWay", "SourceStep", "End"):
            payload = copy.deepcopy(self.payload)
            payload["Members"][0][key] = (
                0 if key == "End" else payload["Members"][0][key] + 1
            )
            self.assertNotEqual(original, self.digest(payload), key)
        for key in ("PathId", "SegmentOrdinal", "Direction"):
            payload = copy.deepcopy(self.payload)
            payload["Members"][0]["Edge"][key] = (
                0 if key == "Direction" else payload["Members"][0]["Edge"][key] + 1
            )
            self.assertNotEqual(original, self.digest(payload), key)

    def test_source_words_original_stencils_ownership_and_signed_zero_are_bound(self):
        original = self.digest()
        for key in (
            "StreetWay",
            "Feature",
            "Ring",
            "PointCount",
            "HalfWidthWord",
            "MaximumGradientWord",
        ):
            payload = copy.deepcopy(self.payload)
            payload["Ways"][0][key] += 1
            self.assertNotEqual(original, self.digest(payload), key)
        for i in range(4):
            for key in ("LongitudeDeg", "LatitudeDeg"):
                payload = copy.deepcopy(self.payload)
                payload["Ways"][0]["EndStencil"][i][key] = math.nextafter(
                    payload["Ways"][0]["EndStencil"][i][key], math.inf
                )
                self.assertNotEqual(original, self.digest(payload), (i, key))
        for i in range(2):
            payload = copy.deepcopy(self.payload)
            payload["Ways"][0]["OwnsEnds"][i] = not payload["Ways"][0]["OwnsEnds"][i]
            self.assertNotEqual(original, self.digest(payload), i)
        payload = copy.deepcopy(self.payload)
        payload["Ways"][0]["EndStencil"][0]["LongitudeDeg"] = 0.0
        positive = self.digest(payload)
        payload["Ways"][0]["EndStencil"][0]["LongitudeDeg"] = -0.0
        self.assertNotEqual(positive, self.digest(payload))

    def test_budget_checks_precede_iterating_records(self):
        with self.assertRaisesRegex(ValueError, "input budget"):
            self.digest(row_limit=0)
        with self.assertRaisesRegex(ValueError, "input budget"):
            self.digest(member_limit=0)

    def test_invalid_words_flags_and_stencils_are_rejected_without_mutation(self):
        before = copy.deepcopy(self.payload)
        self.digest()
        self.assertEqual(before, self.payload)
        cases = []
        payload = copy.deepcopy(self.payload)
        payload["Groups"][0]["Complete"] = 1
        cases.append(payload)
        payload = copy.deepcopy(self.payload)
        payload["SourceGeneration"] = 2**64
        cases.append(payload)
        payload = copy.deepcopy(self.payload)
        payload["Members"][0]["Edge"]["Direction"] = 2
        cases.append(payload)
        payload = copy.deepcopy(self.payload)
        payload["Ways"][0]["EndStencil"][0]["LongitudeDeg"] = float("nan")
        cases.append(payload)
        payload = copy.deepcopy(self.payload)
        payload["Ways"][0]["EndStencil"].pop()
        cases.append(payload)
        for payload in cases:
            with self.assertRaises(ValueError):
                self.digest(payload)


if __name__ == "__main__":
    unittest.main()
