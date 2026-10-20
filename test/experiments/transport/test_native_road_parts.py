"""Fixed-data integrity and native contact-boundary controls."""

import copy
import hashlib
import struct
import unittest

import native_road_parts as corpus


class NativeRoadParts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest, cls.metadata, cls.parts, cls.payload, cls.materials = (
            corpus.load()
        )

    def test_complete_native_junction_halo(self):
        self.assertEqual(len(self.parts), 8358)
        self.assertEqual(corpus.checked_contacts(self.parts, self.metadata), 7479)
        self.assertEqual(
            len(self.materials), self.manifest["files"]["materials.asset"]["bytes"]
        )
        self.assertTrue(
            all(
                r["payload_included"] is False
                for r in self.metadata["boundary_references"]
            )
        )

    def test_cross_part_index_is_rejected_even_with_matching_stream_hash(self):
        part = copy.deepcopy(next(p for p in self.parts if p["counts"][3]))
        size = part["payload_bytes"]
        payload = bytearray(
            self.payload[part["payload_offset"] : part["payload_offset"] + size]
        )
        part["payload_offset"] = 0
        index_begin = sum(part["counts"][:3]) * 4
        struct.pack_into(
            "<I", payload, index_begin, part["base_vertex"] + part["counts"][0] // 3
        )
        part["hashes"]["indices"] = hashlib.sha256(payload[index_begin:]).hexdigest()
        with self.assertRaisesRegex(ValueError, "another part"):
            corpus.checked_payload(part, payload)

    def test_missing_incident_edge_is_not_a_complete_junction(self):
        junction = next(p for p in self.parts if p["kind"] == "junction")
        excluded = junction["legs"][0][0]
        parts = [
            p for p in self.parts if not (p["kind"] == "edge" and p["edge"] == excluded)
        ]
        with self.assertRaisesRegex(ValueError, "incident edge"):
            corpus.checked_contacts(parts, self.metadata)

    def test_matching_topology_cannot_hide_changed_cut_values(self):
        parts = copy.deepcopy(self.parts)
        junction = next(p for p in parts if p["kind"] == "junction")
        junction["gates"][0][2] += 0.01
        with self.assertRaisesRegex(ValueError, "cut gate"):
            corpus.checked_contacts(parts, self.metadata)


if __name__ == "__main__":
    unittest.main()
