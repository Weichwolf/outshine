"""Pinned private native DTO inputs; no product codec or topology reconstruction."""

import gzip
import hashlib
import json
import math
from pathlib import Path

import numpy as np
from outshine_ref.places import decode_terrarium
from outshine_ref.transport.canonical_ports import (
    PortDemand,
    PortNode,
    PortSource,
    PortSpan,
)

FIXTURE = Path(__file__).with_name("fixtures") / "canonical-ports"


def check_file(root, manifest, name):
    expected = manifest["files"][name]
    data = (root / name).read_bytes()
    if (
        len(data) != expected["bytes"]
        or hashlib.sha256(data).hexdigest() != expected["sha256"]
    ):
        raise ValueError("pinned canonical port input changed: " + name)


def load_source(root=FIXTURE):
    """Acquire only pinned source/recipe facts; no DEM read or decode."""
    manifest = json.loads((root / "manifest.json").read_text())
    if manifest["schema"] != "outshine/canonical-port-reference-inputs/1":
        raise ValueError("known captured input schema required")
    for name in ("native.json.gz", "recipes.json"):
        check_file(root, manifest, name)
    with gzip.open(root / "native.json.gz", "rb") as stream:
        raw = stream.read(8 * 1024**2 + 1)
    if (
        len(raw) != manifest["native_raw_bytes"]
        or hashlib.sha256(raw).hexdigest() != manifest["native_raw_sha256"]
    ):
        raise ValueError("captured native DTO bytes changed")
    snapshot = json.loads(raw)
    if (
        snapshot["schema"] != manifest["native_schema"]
        or snapshot["IdentitySpace"] != 1
    ):
        raise ValueError("actual native derived-fragment DTO required")
    recipes = json.loads((root / "recipes.json").read_text())
    source = source_input(snapshot, recipes, manifest)
    return source, manifest


def load(root=FIXTURE):
    source, manifest = load_source(root)
    for name in manifest["files"]:
        if name not in ("native.json.gz", "recipes.json"):
            check_file(root, manifest, name)
    terrain, binding = terrain_input(root, manifest)
    return source, terrain, binding, manifest


def source_input(snapshot, recipes, manifest):
    nodes = {node["SourceNodeId"]: node for node in snapshot["Nodes"]}
    paths = {path["PathId"]: path for path in snapshot["Paths"]}
    if len(nodes) != len(snapshot["Nodes"]) or len(paths) != len(snapshot["Paths"]):
        raise ValueError("native source identities duplicated")
    references = snapshot["References"]
    groups = {}
    for edge in snapshot["Edges"]:
        identity = edge["Id"]
        if identity["Direction"] not in (0, 1):
            raise ValueError("native edge direction changed")
        groups.setdefault((identity["PathId"], identity["SegmentOrdinal"]), []).append(
            edge
        )
    omissions = {
        (row["Source"], tuple(row["Feature"]))
        for row in snapshot["Omissions"]
        if row["Reason"] in (4, 5)
    }
    unknown = set()
    for identifier, node in nodes.items():
        inside = any(
            box["WestDeg"] < node["LongitudeDeg"] < box["EastDeg"]
            and box["SouthDeg"] < node["LatitudeDeg"] < box["NorthDeg"]
            for box in snapshot["Coverage"]
        )
        if not inside:
            unknown.add(identifier)
    spans = []
    for identity, aliases in sorted(groups.items()):
        first = aliases[0]
        original = (first["FromNodeId"], first["ToNodeId"])
        if first["Id"]["Direction"] == 1:
            original = original[::-1]
        for edge in aliases:
            pair = (edge["FromNodeId"], edge["ToNodeId"])
            if edge["Id"]["Direction"] == 1:
                pair = pair[::-1]
            if pair != original or any(
                edge[key] != first[key]
                for key in ("WidthM", "Bridge", "Tunnel", "Layer")
            ):
                raise ValueError("native physical aliases disagree")
        if len({edge["Id"]["Direction"] for edge in aliases}) != len(aliases):
            raise ValueError("native directed alias duplicated")
        path = paths[identity[0]]
        if path["ReferenceCount"] != 1:
            raise ValueError(
                "capture requires one actual source reference per native path"
            )
        reference = references[path["FirstReference"]]
        if not 0 <= reference["Source"] < len(snapshot["Sources"]):
            raise ValueError("native source table reference changed")
        if (
            reference["Source"],
            tuple(path["LocalWayBinding"]["FeatureContent"]),
        ) in omissions:
            unknown.update(original)
        recipe = recipes[str(identity[0])]
        properties = recipe["source_properties"]
        height_qualified = properties.get("indoor") in (
            None,
            False,
            0,
            "no",
        ) and properties.get("level") in (None, 0, "0")
        spans.append(
            PortSpan(
                identity,
                original,
                float(first["WidthM"]),
                recipe["maximum_gradient"] if height_qualified else None,
                first["Bridge"],
                first["Tunnel"],
                {
                    "native_path": path,
                    "native_source_reference": reference,
                    "native_directed_aliases": aliases,
                    "recipe": recipe,
                    "ground_height_recipe_qualified": height_qualified,
                },
                layer=first["Layer"],
            )
        )
    source_nodes = tuple(
        PortNode(identifier, (row["LongitudeDeg"], row["LatitudeDeg"]))
        for identifier, row in sorted(nodes.items())
    )
    return PortSource(
        source_nodes,
        tuple(spans),
        tuple(sorted(unknown)),
        {
            "native_capture": manifest["native_capture"],
            "source_identity": snapshot["SourceIdentity"],
            "sources": snapshot["Sources"],
            "omissions": snapshot["Omissions"],
            "coverage": snapshot["Coverage"],
            "identity_space": snapshot["IdentitySpace"],
            "recipe_source_sha256": manifest["recipe_source_sha256"],
            "scope": "Actual normalized admitted source snapshot; a single tile is not full-world closure",
        },
    )


def terrain_input(root, manifest):
    heights = decode_terrarium((root / "dem.webp").read_bytes())
    heights.setflags(write=False)
    tile = manifest["terrain"]["tile"]

    def terrain(geographic):
        lon, lat = np.asarray(geographic).T
        pixel = np.c_[
            (lon + 180) / 360 * 2 ** tile[0] - tile[1],
            (1 - np.arcsinh(np.tan(np.deg2rad(lat))) / np.pi) / 2 * 2 ** tile[0]
            - tile[2],
        ] * (np.array(heights.shape[::-1]) - 1)
        base = np.floor(pixel).astype(int)
        if np.any(base < 0) or np.any(base >= np.array(heights.shape[::-1]) - 1):
            raise ValueError("missing original DEM stencil")
        u, v = (pixel - base).T
        x, y = base.T
        return (1 - v) * ((1 - u) * heights[y, x] + u * heights[y, x + 1]) + v * (
            (1 - u) * heights[y + 1, x] + u * heights[y + 1, x + 1]
        )

    return terrain, {
        "sources": [manifest["terrain"]],
        "source_datum": manifest["terrain"]["source_datum"],
        "height_mapping": manifest["terrain"]["height_mapping"],
    }


def feature_spans(source, provider_feature, ring):
    return tuple(
        span
        for span in source.spans
        if (
            span.attributes["native_source_reference"]["ProviderElementId"]
            == provider_feature
            and span.attributes["native_path"]["LocalWayBinding"]["FeatureRingOrdinal"]
            == ring
        )
    )


def interior_demands(source, spans, level=20):
    """Only strict interior geographic cuts; native clipping owns other cases."""
    nodes = {n.identifier: n.longitude_latitude for n in source.nodes}
    cells, limits = 2**level, np.array([180, 90])
    steps = np.array([360, 180]) / cells
    demands = []
    for span in spans:
        a, b = [np.array(nodes[node]) for node in span.nodes]
        if abs(b[0] - a[0]) >= 180:
            raise ValueError("native short-dateline clipping required")
        for axis in (0, 1):
            if a[axis] == b[axis]:
                continue
            lo, hi = sorted([a[axis], b[axis]])
            first = math.floor((lo + limits[axis]) / steps[axis]) + 1
            last = math.ceil((hi + limits[axis]) / steps[axis]) - 1
            if last - first > 1024:
                raise ValueError("explicit request enumeration budget")
            for line in range(first, last + 1):
                coordinate = -limits[axis] + line * steps[axis]
                fraction = float((coordinate - a[axis]) / (b[axis] - a[axis]))
                anchor = a + fraction * (b - a)
                anchor[axis] = coordinate
                segment = min(
                    cells - 1,
                    math.floor((anchor[1 - axis] + limits[1 - axis]) / steps[1 - axis]),
                )
                demands.append(
                    PortDemand(
                        span.identifier,
                        fraction,
                        tuple(map(float, anchor)),
                        (level, axis, line, segment),
                    )
                )
    return tuple(demands)
