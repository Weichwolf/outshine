"""Native capture protocol checks; creates controlled inputs, never reference oracles."""

import argparse
import hashlib
import json
import math
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

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
    result = hashlib.sha256(b"outshine/mvt-source-facts/1")
    result.update(native["SourceIdentity"]["Revision"].encode("ascii"))
    result.update(bytes(facts["NodeGroups"]["Digest"]))
    words(
        result,
        "QQQQQ",
        facts["SourceGeneration"],
        facts["SourceWayCount"],
        len(facts["Groups"]),
        len(facts["Members"]),
        len(facts["Ways"]),
    )
    for group in facts["Groups"]:
        words(
            result,
            "IIQQBB",
            group["FirstMember"],
            group["MemberCount"],
            group["OriginalEndpointCount"],
            group["FilteredEndpointCount"],
            group["Complete"],
            group["Frontier"],
        )
    for member in facts["Members"]:
        edge = member["Edge"]
        words(
            result,
            "QIBQIIIB",
            edge["PathId"],
            edge["SegmentOrdinal"],
            edge["Direction"],
            member["DomainNode"],
            member["NodeGroup"],
            member["StreetWay"],
            member["SourceStep"],
            member["End"],
        )
    for way in facts["Ways"]:
        words(
            result,
            "IIIIII",
            *(
                way[key]
                for key in (
                    "StreetWay",
                    "Feature",
                    "Ring",
                    "PointCount",
                    "HalfWidthWord",
                    "MaximumGradientWord",
                )
            ),
        )
        for point in way["EndStencil"]:
            words(result, "dd", point["LongitudeDeg"], point["LatitudeDeg"])
        words(result, "BB", *way["OwnsEnds"])
    return list(result.digest())


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
            terminal = (
                member["SourceStep"] == 0
                if physical[2] == 0
                else member["SourceStep"] == way["PointCount"] - 2
            )
            frontier |= terminal and not way["OwnsEnds"][physical[2]]
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
        and facts["schema"] == "outshine/native-mvt-source-facts/1",
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


def controlled_tile():
    layer = message(1, b"transportation") + bytes((120, 2, 40, 64))
    for key in (b"class", b"layer", b"bridge"):
        layer += message(3, key)
    for value in (b"path", b"0", b"1", b"yes"):
        layer += message(4, message(1, value))
    for identity, tags, geometry in (
        (None, (0, 0, 1, 1), (9, 0, 0, 10, 48, 0)),
        (0, (0, 0, 1, 2, 2, 3), (9, 48, 0, 10, 32, 0)),
    ):
        feature = bytes((24, 2)) + message(2, bytes(tags)) + message(4, bytes(geometry))
        if identity is not None:
            feature += bytes((8, identity))
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
