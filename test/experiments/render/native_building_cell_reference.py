"""Join delivered native cells with explicit unresolved whole-source geometry."""

import gzip
import hashlib
import json

import numpy as np
from check_building_cell_control import check
from compare_directed_groups import transformed
from full_view_reference import load, source_arrays
from parent_surface_inputs import open_parent


def join_sources(parts):
    arrays = {key: [] for key in parts[0]}
    vertices, slots = 0, 0
    for part in parts:
        for key, value in part.items():
            if key == "indices":
                value = value + vertices
            elif key == "binding":
                value = value.copy()
                value[:, 0] += slots
            arrays[key].append(value)
        vertices += len(part["positions"])
        slots += int(part["binding"][:, 0].max()) + 1
    return {key: np.concatenate(value) for key, value in arrays.items()}


def load_cells(manifest_path, archive, cells, frame):
    manifest = json.loads(manifest_path.read_text())
    entries = {entry["cell"]: entry for entry in manifest["cells"]}
    if manifest["version"] != 1 or not cells or not set(cells) <= entries.keys():
        raise ValueError("supported nonempty delivered native cell set required")
    parts, receipts, unresolved = [], [], []
    anchors = []
    with open_parent(archive) as parent:
        if (
            manifest["archive"]["sha256"] != parent.archive_sha256
            or manifest["parent"] != parent.parent
        ):
            raise ValueError("native controls and source archive must match")
        for cell in sorted(cells):
            entry = entries[cell]
            path = manifest_path.parent / entry["file"]
            raw = path.read_bytes()
            if (
                len(raw) != entry["bytes"]
                or hashlib.sha256(raw).hexdigest() != entry["sha256"]
            ):
                raise ValueError("native control hash mismatch")
            document = json.loads(gzip.decompress(raw))
            receipt = check(document, parent.plans["records"], parent.parent)
            if (
                receipt["cell"] != cell
                or receipt["unsupported"] != entry["unsupported"]
                or receipt["triangles"] != entry["triangles"]
                or receipt["sources"] != entry["sources"]
            ):
                raise ValueError("native control counts/failures differ from manifest")
            anchors.append(document["anchor_ecef"])
            if cell == 33:
                source, _, _, replay = load(path)
                receipt["archive_replay"] = replay
            else:
                source = source_arrays(document["results"][0])
            parts.append(source)
            unresolved.extend(receipt["unsupported"])
            receipts.append(
                receipt
                | {
                    "file": entry["file"],
                    "bytes": len(raw),
                    "sha256": entry["sha256"],
                    "kind": "archived geometry replay"
                    if cell == 33
                    else "native producer control",
                }
            )
    if any(anchor != anchors[0] for anchor in anchors):
        raise ValueError("native cells require a common original anchor")
    source = join_sources(parts)
    known = set(map(int, np.unique(source["binding"][:, 1])))
    missing = {f["source"] for f in unresolved}
    if known & missing or known | missing != set(map(int, frame["selected_sources"])):
        raise ValueError(
            "every selected original must be geometry or an explicit failure"
        )
    decoded_bytes = sum(v.nbytes for v in source.values())
    source = transformed(source, frame["basis"], frame["anchor"])
    return source, {
        "native_product_commit": manifest["native_product_commit"],
        "native_product_tree": manifest["native_product_tree"],
        "source_archive_sha256": manifest["archive"]["sha256"],
        "cells": receipts,
        "unresolved_originals": unresolved,
        "decoded_array_bytes_before_transform": decoded_bytes,
        "decoded_array_bytes_after_transform": sum(v.nbytes for v in source.values()),
        "scope": "Delivered unprojected native geometry with current-policy source-material model. Explicit failed originals have no geometry: image excludes them and their actual price remains unknown. Reference CPU arrays/control files are not native asset IO or GPU upload prices.",
    }
