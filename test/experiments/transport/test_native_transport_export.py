"""Native capture protocol checks; creates controlled inputs, never reference oracles."""

import argparse
import gzip
import hashlib
import json
import math
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

from mvt_source_fact_digest import (
    native_source_fact_digest,
    validate_missing_source_ways,
)

ROOT = Path(__file__).resolve().parents[3]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def words(hash_value, format_string, *values):
    hash_value.update(struct.pack("<" + format_string, *values))


def group_digest(native, facts):
    groups = facts["NodeGroups"]["Groups"]
    nodes = facts["NodeGroups"]["Nodes"]
    result = hashlib.sha256(b"outshine/mvt-node-groups/1")
    result.update(native["SourceIdentity"]["Revision"].encode("ascii"))
    words(result, "QQ", len(groups), len(nodes))
    for group in groups:
        words(result, "IQ", group["NodeCount"], group["SourceConnectionCount"])
        for node in nodes[group["FirstNode"] : group["FirstNode"] + group["NodeCount"]]:
            words(result, "Q", node)
    return list(result.digest())


def fact_digest(native, facts):
    return list(
        native_source_fact_digest(
            native["SourceIdentity"]["Revision"],
            bytes(facts["NodeGroups"]["Digest"]),
            facts,
        )
    )


def edge_key(edge):
    return edge["PathId"], edge["SegmentOrdinal"], edge["Direction"]


def check_members(native, facts):
    nodes = {
        n["SourceNodeId"]: (n["LongitudeDeg"], n["LatitudeDeg"])
        for n in native["Nodes"]
    }
    edges = {edge_key(e["Id"]): e for e in native["Edges"]}
    paths = {p["PathId"]: p for p in native["Paths"]}
    ways = {w["StreetWay"]: w for w in facts["Ways"]}
    require(len(ways) == len(facts["Ways"]), "duplicate source Way")
    expected = {}
    for key, edge in edges.items():
        for end in (0, 1):
            physical = (*key[:2], end ^ key[2])
            node = edge["FromNodeId" if end == 0 else "ToNodeId"]
            require(
                physical not in expected or expected[physical] == node,
                "directed aliases disagree",
            )
            expected[physical] = node
    seen = {}
    offset = 0
    all_nodes = []
    previous = 0
    require(
        len(facts["Groups"]) == len(facts["NodeGroups"]["Groups"]),
        "partition/group size mismatch",
    )
    for at, group in enumerate(facts["Groups"]):
        partition = facts["NodeGroups"]["Groups"][at]
        ids = facts["NodeGroups"]["Nodes"][
            partition["FirstNode"] : partition["FirstNode"] + partition["NodeCount"]
        ]
        require(
            ids and ids == sorted(set(ids)) and ids[0] > previous,
            "noncanonical partition",
        )
        previous = ids[0]
        require(
            all(nodes[n] == nodes[ids[0]] for n in ids),
            "source domain positions disagree",
        )
        all_nodes.extend(ids)
        require(
            group["FirstMember"] == offset and group["MemberCount"] > 0,
            "invalid member CSR",
        )
        require(
            group["OriginalEndpointCount"] >= group["MemberCount"],
            "lost original incidence",
        )
        filtered = group["OriginalEndpointCount"] - group["MemberCount"]
        require(
            group["FilteredEndpointCount"] == filtered
            and group["Complete"] == (filtered == 0),
            "false completeness",
        )
        frontier = False
        for member in facts["Members"][offset : offset + group["MemberCount"]]:
            key = edge_key(member["Edge"])
            require(
                member["End"] in (0, 1) and key[2] in (0, 1), "invalid directed end"
            )
            physical = (*key[:2], member["End"] ^ key[2])
            require(
                key in edges and physical not in seen,
                "missing edge or repeated physical end",
            )
            require(
                member["NodeGroup"] == at
                and member["DomainNode"] in ids
                and expected[physical] == member["DomainNode"],
                "wrong directed/domain binding",
            )
            path = paths[key[0]]
            local = path["LocalWayBinding"]
            way = ways[member["StreetWay"]]
            require(
                local["StreetWay"] == way["StreetWay"]
                and local["Feature"] == way["Feature"]
                and local["Ring"] == way["Ring"],
                "wrong source Way scope",
            )
            reference = native["References"][path["FirstReference"]]
            require(
                reference["SegmentOrdinal"] == member["SourceStep"]
                and 0 <= member["SourceStep"] < way["PointCount"] - 1,
                "wrong source step",
            )
            width = struct.unpack("<f", struct.pack("<I", way["HalfWidthWord"]))[0]
            require(
                math.isfinite(width)
                and width > 0
                and edges[key]["WidthM"] == 2 * width,
                "wrong native half-width word",
            )
            require(
                type(member["SourceTerminal"]) is bool,
                "missing native terminal witness",
            )
            frontier |= member["SourceTerminal"] and not way["OwnsEnds"][physical[2]]
            seen[physical] = member["DomainNode"]
        require(group["Frontier"] == frontier, "frontier binding changed")
        offset += group["MemberCount"]
    require(
        offset == len(facts["Members"]) and seen == expected,
        "physical endpoint coverage changed",
    )
    require(
        sorted(all_nodes) == sorted(nodes), "partition lost or repeated published nodes"
    )


def check_capture(directory):
    native = json.loads((directory / "native.json").read_text())
    facts = json.loads((directory / "source-facts.json").read_text())
    require(
        native["schema"] == "outshine/native-mvt-snapshot-dump/1"
        and facts["schema"]
        in (
            "outshine/native-mvt-source-facts/2",
            "outshine/native-mvt-source-facts/3",
            "outshine/native-mvt-source-facts/4",
        ),
        "unsupported native capture",
    )
    require(
        native["SourceIdentity"] == facts["NetworkIdentity"]
        and native["input_sha256"] == facts["InputSha256"]
        and native["tile"] == facts["Tile"]
        and native["SourceWayCount"] == facts["SourceWayCount"]
        and native["SourceGeneration"] == facts["SourceGeneration"],
        "different native/source scope",
    )
    require(
        facts["NodeGroups"]["Digest"] == group_digest(native, facts),
        "partition binding changed",
    )
    require(
        facts["Digest"] == fact_digest(native, facts), "source fact binding changed"
    )
    validate_missing_source_ways(native, facts)
    check_members(native, facts)
    return native, facts


def varint(value):
    result = bytearray()
    while value > 127:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return bytes(result)


def message(field, value):
    return varint(field * 8 + 2) + varint(len(value)) + value


def controlled_tile(profile_tags=False, omitted_tunnel=False):
    layer = message(1, b"transportation") + bytes((120, 2, 40, 64))
    for key in (b"class", b"layer", b"bridge", b"indoor", b"level"):
        layer += message(3, key)
    if omitted_tunnel:
        layer += message(3, b"tunnel")
    for value in (b"path", b"0", b"1", b"yes"):
        layer += message(4, message(1, value))
    if profile_tags:
        layer += message(4, bytes((25,)) + struct.pack("<d", -0.0))
        layer += message(4, message(1, b"1"))
    for identity, tags, geometry in (
        (None, (0, 0, 1, 1), (9, 0, 0, 10, 48, 0)),
        (0, (0, 0, 1, 2, 2, 3), (9, 48, 0, 10, 32, 0)),
    ):
        if profile_tags and identity is None:
            tags = (*tags, 3, 4, 4, 5)
        feature = bytes((24, 2)) + message(2, bytes(tags)) + message(4, bytes(geometry))
        if identity is not None:
            feature += bytes((8, identity))
        layer += message(2, feature)
    if omitted_tunnel:
        feature = bytes((24, 2)) + message(2, bytes((0, 0, 5, 3)))
        feature += message(4, bytes((9, 16, 32, 10, 16, 0)))
        layer += message(2, feature)
    return message(3, layer)


class NativeExport(unittest.TestCase):
    exporter = None

    def setUp(self):
        self.work = tempfile.TemporaryDirectory(prefix="outshine-native-source-export-")
        self.addCleanup(self.work.cleanup)
        self.root = Path(self.work.name)
        self.input = self.root / "input.mvt"
        self.input.write_bytes(controlled_tile())

    def run_export(self, directory, zoom="14"):
        return subprocess.run(
            [str(self.exporter), str(self.input), zoom, "8192", "8192", str(directory)],
            cwd=ROOT,
            check=False,
            capture_output=True,
            text=True,
            timeout=20,
        )

    def test_actual_dto_and_digest_roundtrip(self):
        directory = self.root / "capture"
        result = self.run_export(directory)
        self.assertEqual(result.returncode, 0, result.stderr)
        native, facts = check_capture(directory)
        self.assertEqual(len(native["Nodes"]), 4)
        self.assertEqual(len(facts["Groups"]), 3)
        self.assertEqual(len(facts["Members"]), 4)
        self.assertEqual(
            {r["ProviderElementId"] for r in native["References"]}, {None, 0}
        )
        self.assertTrue(
            any(
                point["LatitudeDeg"] == 0
                for w in facts["Ways"]
                for point in w["EndStencil"]
            )
        )
        facts["Ways"][0]["MaximumGradientWord"] += 1
        (directory / "source-facts.json").write_text(json.dumps(facts))
        with self.assertRaisesRegex(ValueError, "source fact binding"):
            check_capture(directory)

    def test_original_height_tags_remain_typed(self):
        self.input.write_bytes(controlled_tile(profile_tags=True))
        directory = self.root / "typed-tags"
        result = self.run_export(directory)
        self.assertEqual(result.returncode, 0, result.stderr)
        native, _ = check_capture(directory)
        rows = [p["LocalWayBinding"]["HeightProfileTags"] for p in native["Paths"]]
        typed = [r for r in rows if r["indoor"] is not None]
        absent = [r for r in rows if r["indoor"] is None]
        self.assertEqual(len(typed), 1)
        self.assertTrue(typed[0]["indoor"]["IsNumber"])
        self.assertEqual(typed[0]["indoor"]["NumberWord"], 1 << 63)
        self.assertFalse(typed[0]["level"]["IsNumber"])
        self.assertEqual(typed[0]["level"]["String"], "1")
        self.assertEqual(absent, [{"indoor": None, "level": None}])

    def test_way_identity_and_feature_wide_omission_scope_survive_export(self):
        self.input.write_bytes(controlled_tile(omitted_tunnel=True))
        directory = self.root / "omissions"
        result = self.run_export(directory)
        self.assertEqual(result.returncode, 0, result.stderr)
        native, _ = check_capture(directory)
        ambiguous = [row for row in native["Omissions"] if row["Reason"] == 4]
        self.assertEqual(len(ambiguous), 2)
        self.assertEqual(
            {tuple(row["Fragment"]) for row in ambiguous},
            {tuple(row["Fragment"]) for row in native["References"]},
        )
        feature_wide = [row for row in native["Omissions"] if row["Reason"] == 1]
        self.assertEqual(len(feature_wide), 1)
        self.assertIsNone(feature_wide[0]["Fragment"])

    def test_retained_actual_source_pair(self):
        fixture = Path(__file__).with_name("fixtures") / "bridge-source-facts"
        manifest = json.loads((fixture / "manifest.json").read_text())
        for name, metadata in manifest["files"].items():
            compressed = (fixture / name).read_bytes()
            self.assertEqual(len(compressed), metadata["bytes"])
            self.assertEqual(hashlib.sha256(compressed).hexdigest(), metadata["sha256"])
            raw = gzip.decompress(compressed)
            self.assertEqual(len(raw), metadata["raw_bytes"])
            self.assertEqual(hashlib.sha256(raw).hexdigest(), metadata["raw_sha256"])
            (self.root / name.removesuffix(".gz")).write_bytes(raw)
        native, facts = check_capture(self.root)
        self.assertEqual(native["SourceIdentity"], manifest["source_identity"])
        self.assertEqual(
            facts["InputSha256"],
            hashlib.sha256((self.root / "source.mvt").read_bytes()).hexdigest(),
        )
        self.assertEqual(facts["SourceGeneration"], manifest["source_generation"])
        self.assertEqual(facts["SourceWayCount"], manifest["source_way_count"])

    def test_invalid_requests_and_output_preservation(self):
        directory = self.root / "existing"
        directory.mkdir()
        marker = directory / "marker"
        marker.write_text("keep")
        self.assertNotEqual(self.run_export(directory).returncode, 0)
        self.assertEqual(marker.read_text(), "keep")
        self.assertNotEqual(
            self.run_export(self.root / "bad-zoom", "invalid").returncode, 0
        )
        self.input.write_bytes(b"not MVT")
        self.assertNotEqual(self.run_export(self.root / "bad-input").returncode, 0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--exporter", type=Path, required=True)
    args, remaining = parser.parse_known_args()
    NativeExport.exporter = args.exporter.resolve()
    unittest.main(argv=[__file__, *remaining])
