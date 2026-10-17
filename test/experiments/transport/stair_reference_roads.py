"""Complete control geometry and facet indices from verified asset-local owners."""

import hashlib

import numpy as np


def reference_roads(assets):
    keys = [key for asset in assets for key in asset.source_ids]
    if not 1 <= len(assets) <= 16 or len(set(keys)) != len(keys):
        raise ValueError("bounded distinct complete-control assets required")
    if sum(len(a.data["body_faces"]) for a in assets) > 32768:
        raise ValueError("complete-control body face budget exhausted")
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
            "source_mask_scope": "No global mask: complete control reads verified asset-local owners through explicit source facet indices. No generator budget change.",
        },
    )


def reference_owner_indices(assets):
    offset, result = 0, {}
    for asset in assets:
        for column, key in enumerate(asset.source_ids):
            indices = np.flatnonzero(asset.data["owners"][:, column]) + offset
            indices.flags.writeable = False
            result[key] = indices
        offset += len(asset.data["top_faces"])
    return result
