"""Load the actual #1488 paired source inputs; never read DEM or admit contacts."""

import gzip
import hashlib
import io
import json
import math
from pathlib import Path

from canonical_port_native_inputs import source_input
from mvt_node_group_inputs import read_mvt_node_groups, unsigned
from mvt_source_fact_digest import native_source_fact_digest, version_from_schema
from mvt_source_fact_links import check_mvt_source_fact_links
from mvt_source_profile_inputs import check_height_profile_tags, profile_properties
from outshine_ref.contracts import immutable
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._end_candidates import (
    CandidateGroup,
    CandidateMember,
    CandidateWay,
    float_word,
)


def checked_file(root, manifest, name):
    info = manifest["files"][name]
    if (
        not unsigned(info["bytes"], 64)
        or not unsigned(info["raw_bytes"], 64)
        or info["bytes"] > 4 * 1024 * 1024
        or info["raw_bytes"] > 16 * 1024 * 1024
    ):
        raise ValueError("paired capture file budget")
    path = root / name
    if path.stat().st_size != info["bytes"]:
        raise ValueError("paired capture compressed byte count")
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != info["sha256"]:
        raise ValueError("paired capture compressed digest")
    with gzip.GzipFile(fileobj=io.BytesIO(data)) as stream:
        raw = stream.read(info["raw_bytes"] + 1)
    if (
        len(raw) != info["raw_bytes"]
        or hashlib.sha256(raw).hexdigest() != info["raw_sha256"]
    ):
        raise ValueError("paired capture raw bytes/digest")
    return raw


def check_scope(native, facts, manifest, budget):
    if (
        manifest["schema"] != "outshine/native-mvt-source-reference/1"
        or native["schema"] != "outshine/native-mvt-snapshot-dump/1"
        or facts["schema"]
        not in (
            "outshine/native-mvt-source-facts/1",
            "outshine/native-mvt-source-facts/2",
        )
    ):
        raise ValueError("actual paired source schemas required")
    if native["mvt_schema"] != "openmaptiles" or len(native["Sources"]) != 1:
        raise ValueError("only actual single-source OpenMapTiles export supported")
    for key, other, saved in (
        ("SourceIdentity", "NetworkIdentity", "source_identity"),
        ("input_sha256", "InputSha256", "input_sha256"),
        ("tile", "Tile", "tile"),
    ):
        if native[key] != facts[other] or native[key] != manifest[saved]:
            raise ValueError("paired source scope mismatch: " + key)
    if (
        manifest["native_schema"] != native["schema"]
        or manifest["source_facts_schema"] != facts["schema"]
    ):
        raise ValueError("paired capture declared schema mismatch")
    for key, saved in (
        ("SourceGeneration", "source_generation"),
        ("SourceWayCount", "source_way_count"),
    ):
        if (
            not unsigned(native[key], 64)
            or not unsigned(manifest[saved], 64)
            or type(facts[key]) is not int
            or native[key] != facts[key]
            or native[key] != manifest[saved]
        ):
            raise ValueError("independent snapshot scope mismatch: " + key)
    for rows, limit in (
        (native["Nodes"], budget.input_nodes),
        (native["Paths"], budget.input_spans),
        (native["Edges"], budget.incidence_visits),
        (facts["Groups"], budget.input_nodes),
        (facts["Ways"], budget.input_spans),
        (facts["Members"], budget.incidence_visits),
    ):
        if len(rows) > limit:
            raise ValueError("paired source input budget")
    digest = native_source_fact_digest(
        native["SourceIdentity"]["Revision"],
        bytes(facts["NodeGroups"]["Digest"]),
        facts,
        member_limit=budget.incidence_visits,
        row_limit=max(budget.input_nodes, budget.input_spans),
        version=version_from_schema(facts["schema"]),
    )
    if list(digest) != facts["Digest"]:
        raise ValueError("paired native fact digest mismatch")


def native_recipes(native, facts, raw):
    properties, work = profile_properties(raw)
    ways = {row["StreetWay"]: row for row in facts["Ways"]}
    if len(ways) != len(facts["Ways"]):
        raise ValueError("duplicate native Way facts")
    recipes = {}
    for path in native["Paths"]:
        local = path["LocalWayBinding"]
        if (
            local["LayerName"] != "streets"
            or bytes(local["FeatureContent"]) not in properties
        ):
            raise ValueError("actual source feature content binding missing")
        if (
            path["ReferenceCount"] != 1
            or native["References"][path["FirstReference"]]["Source"] != 0
        ):
            raise ValueError("actual single-source path reference required")
        way = ways[local["StreetWay"]]
        if facts["schema"] == "outshine/native-mvt-source-facts/2":
            check_height_profile_tags(
                local.get("HeightProfileTags"),
                properties[bytes(local["FeatureContent"])],
            )
        grade = float_word(way["MaximumGradientWord"])
        if not math.isfinite(grade) or not 0 < grade <= 10:
            grade = None
        recipes[str(path["PathId"])] = {
            "source_properties": properties[bytes(local["FeatureContent"])],
            "maximum_gradient": grade,
            "origin": "Actual binary32 Way.MaxGradient plus raw feature-content-bound native indoor/level semantics",
            "native_half_width_word": way["HalfWidthWord"],
            "native_maximum_gradient_word": way["MaximumGradientWord"],
        }
    work["bound_profile_paths"] = len(recipes)
    work["checked_height_profile_tag_records"] = (
        2 * len(recipes)
        if facts["schema"] == "outshine/native-mvt-source-facts/2"
        else 0
    )
    return recipes, work


def candidate_values(source, partition, facts):
    positions = {node.identifier: node.longitude_latitude for node in source.nodes}
    qualified = {}
    for span in source.spans:
        way = span.attributes["native_path"]["LocalWayBinding"]["StreetWay"]
        flag = (
            span.attributes["ground_height_recipe_qualified"]
            and span.maximum_gradient is not None
        )
        if way in qualified and qualified[way] != flag:
            raise ValueError("same native Way has conflicting profile qualification")
        qualified[way] = flag
    ways = []
    for row in facts["Ways"]:
        points = tuple((p["LongitudeDeg"], p["LatitudeDeg"]) for p in row["EndStencil"])
        ways.append(
            CandidateWay(
                row["StreetWay"],
                row["PointCount"],
                points[:2],
                points[2:],
                tuple(row["OwnsEnds"]),
                row["HalfWidthWord"],
                row["MaximumGradientWord"] if qualified[row["StreetWay"]] else None,
            )
        )
    digest = bytes(facts["Digest"]).hex()
    groups = []
    for index, row in enumerate(facts["Groups"]):
        members = []
        for member in facts["Members"][
            row["FirstMember"] : row["FirstMember"] + row["MemberCount"]
        ]:
            edge = member["Edge"]
            members.append(
                CandidateMember(
                    (edge["PathId"], edge["SegmentOrdinal"], edge["Direction"]),
                    member["End"],
                    member["DomainNode"],
                    member["StreetWay"],
                    member["SourceTerminal"]
                    if facts["schema"] == "outshine/native-mvt-source-facts/2"
                    else None,
                )
            )
        anchor = partition.nodes[partition.groups[index].first_node]
        groups.append(
            CandidateGroup(
                digest + ":" + str(index),
                positions[anchor],
                tuple(members),
                row["OriginalEndpointCount"],
                row["FilteredEndpointCount"],
                row["Complete"],
                row["Frontier"],
            )
        )
    return tuple(groups), tuple(ways)


def load_bridge_source_facts(root, *, budget=None):
    """Explicit actual input directory; no old reference fallback or terrain IO.

    The native CLI emits this wrapper only after successful owned fact capture.
    A legacy/missing/budget-limited capture cannot masquerade as this schema.
    Complete is relative to the supplied native Network, never global closure.
    """
    budget = PortBudget() if budget is None else budget
    root = Path(root)
    if (root / "manifest.json").stat().st_size > 32768:
        raise ValueError("paired source manifest budget")
    manifest = json.loads((root / "manifest.json").read_bytes())
    native = json.loads(checked_file(root, manifest, "native.json.gz"))
    facts = json.loads(checked_file(root, manifest, "source-facts.json.gz"))
    check_scope(native, facts, manifest, budget)
    raw = checked_file(root, manifest, "source.mvt.gz")
    if hashlib.sha256(raw).hexdigest() != manifest["input_sha256"]:
        raise ValueError("original MVT input scope mismatch")
    if native["Sources"][0]["Content"] != list(hashlib.sha256(raw).digest()):
        raise ValueError("original MVT and actual native source content differ")
    recipes, profile_work = native_recipes(native, facts, raw)
    binding = {
        "native_capture": manifest,
        "source_identity": native["SourceIdentity"],
        "sources": native["Sources"],
        "omissions": native["Omissions"],
        "coverage": native["Coverage"],
        "identity_space": native["IdentitySpace"],
        "source_generation": native["SourceGeneration"],
        "source_way_count": native["SourceWayCount"],
        "source_facts_digest": bytes(facts["Digest"]).hex(),
        "scope": "Actual paired native source; physical incidence only within supplied Network, no geographic/ground/contact closure",
    }
    source = source_input(native, recipes, manifest, binding=binding)
    partition, reason, partition_work = read_mvt_node_groups(
        source, 1, facts["NodeGroups"], budget=budget
    )
    if reason:
        raise ValueError(reason)
    reason, link_work = check_mvt_source_fact_links(
        source, partition, facts, budget=budget
    )
    if reason:
        raise ValueError(reason)
    groups, ways = candidate_values(source, partition, facts)
    candidate_binding = {
        "schema": facts["schema"],
        "network_identity": facts["NetworkIdentity"],
        "source_generation": facts["SourceGeneration"],
        "source_way_count": facts["SourceWayCount"],
        "node_groups_digest": partition.digest.hex(),
        "source_facts_digest": bytes(facts["Digest"]).hex(),
        "source_payload_sha256": manifest["input_sha256"],
    }
    return (
        source,
        groups,
        ways,
        immutable(candidate_binding),
        {"profiles": profile_work, "partition": partition_work, "links": link_work},
    )
