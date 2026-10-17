"""Complete seed-control geometry; original owners stay in verified local assets."""

import hashlib

import numpy as np


def seed_reference_roads(assets):
    keys = [key for asset in assets for key in asset.source_ids]
    if not 1 <= len(assets) <= 16 or len(set(keys)) != len(keys):
        raise ValueError("bounded distinct complete seed-control assets required")
    if sum(len(a.data["body_faces"]) for a in assets) > 32768:
        raise ValueError("complete seed-control body face budget exhausted")
    columns = {
        key: [] for key in ("body_vertices", "body_faces", "top_faces", "bottom_faces")
    }
    offset, owners = 0, []
    for asset in assets:
        for key, column in columns.items():
            value = asset.data[key]
            column.append(value if key == "body_vertices" else value + offset)
        offset += len(asset.data["body_vertices"])
        owners.append(
            {
                "identity": asset.identity,
                "source_ids": list(asset.source_ids),
                "owner_shape": list(asset.data["owners"].shape),
                "owner_sha256": hashlib.sha256(
                    asset.data["owners"].tobytes()
                ).hexdigest(),
            }
        )
    road = {key: np.vstack(values) for key, values in columns.items()}
    return (
        road,
        np.concatenate([a.data["ground"] for a in assets]),
        keys,
        {row["source_id"]: row for asset in assets for row in asset.rows},
        {
            "complete_assets": owners,
            "complete_sources": len(keys),
            "complete_body_faces": len(road["body_faces"]),
            "geometry_array_bytes": sum(v.nbytes for v in road.values()),
            "source_mask_scope": "No global mask: seed control reads geometry only; verified original owners remain asset-local. Not an attached-module control or generator budget change.",
        },
    )
