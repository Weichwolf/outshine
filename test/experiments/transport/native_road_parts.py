#!/usr/bin/env python3
"""Read and verify the fixed native semantic road corpus; no runtime page schema."""

import argparse
import hashlib
import json
import math
import struct
import tarfile
from pathlib import Path

FILES = {"metadata.json", "parts.jsonl", "payloads.bin", "materials.asset"}
DEFAULT_INPUT = (
    Path(__file__).resolve().parents[2] / "outshine/data/transport/native_road_parts"
)


def checked_payload(part, payload):
    """Return original position, normal, colour and index words without copying."""
    counts = part["counts"]
    if len(counts) != 4 or any(type(n) is not int or n < 0 for n in counts):
        raise ValueError("invalid native stream counts")
    positions, normals, colours, indices = counts
    if (
        positions % 3
        or normals != positions
        or colours * 3 != positions * 4
        or indices % 3
    ):
        raise ValueError("invalid native mesh tuples")
    begin, size = part["payload_offset"], part["payload_bytes"]
    if begin < 0 or size != sum(counts) * 4 or begin + size > len(payload):
        raise ValueError("native payload range exceeds the fixed bytes")
    words = memoryview(payload)[begin : begin + size]
    streams = {}
    cursor = 0
    for name, count in zip(("positions", "normals", "colours", "indices"), counts):
        stream = words[cursor : cursor + count * 4]
        if hashlib.sha256(stream).hexdigest() != part["hashes"][name]:
            raise ValueError("native stream hash differs")
        streams[name] = stream
        cursor += count * 4
    base = part["base_vertex"]
    vertices = positions // 3
    if any(
        not base <= value < base + vertices
        for (value,) in struct.iter_unpack("<I", streams["indices"])
    ):
        raise ValueError("native part references another part's vertices")
    for name in ("positions", "normals", "colours"):
        if any(
            not math.isfinite(value)
            for (value,) in struct.iter_unpack("<f", streams[name])
        ):
            raise ValueError("nonfinite native word")
    return streams


def checked_contacts(parts, metadata):
    """Check the complete captured junction halo against original edge ports."""
    edges = {part["edge"]: part for part in parts if part["kind"] == "edge"}
    junctions = [part for part in parts if part["kind"] == "junction"]
    if len(edges) + len(junctions) != len(parts):
        raise ValueError("duplicate or unknown native part identity")
    nodes = set()
    legs = 0
    for junction in junctions:
        if junction["node"] in nodes or len(junction["legs"]) != len(junction["gates"]):
            raise ValueError("duplicate junction or incomplete gates")
        nodes.add(junction["node"])
        for leg, gate in zip(junction["legs"], junction["gates"]):
            edge_id, end = leg[:2]
            if edge_id not in edges or end not in (0, 1):
                raise ValueError("captured junction lacks an incident edge payload")
            edge = edges[edge_id]
            port = edge["ports"][end]
            if edge["nodes"][end] != junction["node"] or port is None:
                raise ValueError("junction does not bind its original endpoint")
            if port["gate"] != gate or port["plane"] != junction["plane"]:
                raise ValueError("original native cut gate or plane differs")
            legs += 1
    referenced = list(edges.values()) + [
        r for r in metadata["boundary_references"] if r["kind"] == "edge"
    ]
    for edge in referenced:
        lane = metadata["lanes"].get(str(edge["lane"]))
        if lane is None or not 0 <= lane["source_tile"] < len(metadata["sources"]):
            raise ValueError("native edge lacks its original source scope")
        provider = lane["provider_feature_id"]
        if provider is not None and (
            type(provider) is not int or not 0 <= provider < 2**64
        ):
            raise ValueError("provider ID was narrowed or invented")
    return legs


def load(root=DEFAULT_INPUT):
    """Load pinned input atoms; they are not independently prepared render LODs."""
    root = Path(root)
    manifest = json.loads((root / "manifest.json").read_text())
    if manifest["schema"] != 1 or set(manifest["files"]) != FILES:
        raise ValueError("unsupported fixed native corpus")
    archive = root / manifest["archive"]
    if archive.stat().st_size != manifest["archive_bytes"]:
        raise ValueError("archive length differs")
    if hashlib.sha256(archive.read_bytes()).hexdigest() != manifest["archive_sha256"]:
        raise ValueError("archive hash differs")
    files = {}
    with tarfile.open(archive, "r:xz") as packed:
        members = packed.getmembers()
        if len(members) != len(FILES) or {m.name for m in members} != FILES:
            raise ValueError("unexpected fixed archive members")
        for member in members:
            expected = manifest["files"][member.name]
            if not member.isfile() or member.size != expected["bytes"]:
                raise ValueError("invalid fixed archive member")
            content = packed.extractfile(member).read()
            if hashlib.sha256(content).hexdigest() != expected["sha256"]:
                raise ValueError("fixed member hash differs")
            files[member.name] = content
    metadata = json.loads(files["metadata.json"])
    parts = [json.loads(row) for row in files["parts.jsonl"].splitlines()]
    cursor = 0
    for part in parts:
        if part["payload_offset"] != cursor:
            raise ValueError("payload is overlapping or leaves unowned bytes")
        checked_payload(part, files["payloads.bin"])
        cursor += part["payload_bytes"]
    if cursor != len(files["payloads.bin"]):
        raise ValueError("unowned trailing payload")
    if sum(p["kind"] == "edge" for p in parts) != manifest["payload_edges"]:
        raise ValueError("edge count differs")
    if sum(p["kind"] == "junction" for p in parts) != manifest["payload_junctions"]:
        raise ValueError("junction count differs")
    checked_contacts(parts, metadata)
    checked_contacts(parts + metadata["boundary_references"], metadata)
    return manifest, metadata, parts, files["payloads.bin"], files["materials.asset"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT)
    args = parser.parse_args()
    manifest, metadata, parts, payload, _ = load(args.input)
    legs = checked_contacts(parts, metadata)
    print(
        json.dumps(
            {
                "parts": len(parts),
                "edges": manifest["payload_edges"],
                "junctions": manifest["payload_junctions"],
                "native_gate_bindings": legs,
                "payload_bytes": len(payload),
                "scope": manifest["scope"],
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
