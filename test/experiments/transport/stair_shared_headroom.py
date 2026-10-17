"""Bind logical car headroom to the actual previously built walking cap."""

import hashlib
import json
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from shared_bridge_geometry import corridor
from stair_module_inputs import reference_profiles


def binding(new, old, contact):
    key, old_key = contact["source_id"], contact["cached_source_id"]
    if (
        key not in new.source_ids
        or old_key not in old.source_ids
        or contact["asset_sha256"] != old.identity
        or contact["access"]["kind"] != "minor_footway_node"
        or contact["access"]["motor_cycle_contact_arcs"] != 0
    ):
        raise ValueError("verified logical road/old footway owner binding required")
    column = old.source_ids.index(old_key)
    indices = np.flatnonzero(old.data["owners"][:, column])
    xyz = old.data["body_vertices"][old.data["top_faces"][indices]]
    if (
        contact["asset_owner_column"] != column
        or contact["asset_top_face_indices"] != indices.tolist()
        or contact["top_xyz_sha256"] != hashlib.sha256(xyz.tobytes()).hexdigest()
        or not old.data["ground"][indices].all()
    ):
        raise ValueError("whole immutable old headroom cap binding changed")
    row = next(r for r in new.rows if r["source_id"] == key)
    required = row.get("traffic_profile", {}).get("clearance_m")
    if (
        row.get("profile_kind") != "unclassified"
        or required != reference_profiles()["unclassified"]["clearanceM"]
    ):
        raise ValueError("explicit full new carriageway headroom required")
    domain = corridor(row)
    mask = shapely.union_all(old.shapes[indices]).intersection(domain)
    if (
        mask.is_empty
        or not mask.is_valid
        or mask.geom_type not in ("Polygon", "MultiPolygon")
        or not mask.equals(shapely.from_wkt(contact["mask_wkt"]))
    ):
        raise ValueError("whole borrowed headroom mask differs from its actual source")
    return {
        "source_id": key,
        "old_source_id": old_key,
        "required_m": required,
        "old_asset": old,
        "indices": indices,
        "domain": domain,
        "mask": mask,
    }


def bindings(assets, modules):
    by_id = {a.identity: a for a in assets}
    result, seen = [], set()
    for path in modules:
        path = Path(path)
        report = json.loads((path / "comparison.json").read_text())
        if not report["valid_local_module"]:
            continue
        new = by_id[digest(path / "module.npz")]
        records = report["proof"]["module"].get("surface_contacts", [])
        for contact in records:
            if contact["access"]["kind"] != "minor_footway_node":
                continue
            pair = (contact["source_id"], contact["cached_source_id"])
            old = by_id[contact["asset_sha256"]]
            if pair in seen or assets.index(old) >= assets.index(new):
                raise ValueError("unique prior immutable headroom owner required")
            seen.add(pair)
            result.append(binding(new, old, contact))
            if len(result) > 128:
                raise ValueError("borrowed headroom binding budget exhausted")
    return tuple(result)
