"""Resident research road bodies with asset-local owners and bounded working copies."""

import json
from dataclasses import dataclass
from pathlib import Path
from types import MappingProxyType

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.geometry.surface_patches._patch import readonly
from stair_module_inputs import read_arrays
from stair_patch_inputs import load_prepared
from wien_support_verify import unpack


@dataclass(frozen=True, slots=True)
class RoadAsset:
    identity: str
    source_ids: tuple
    rows: tuple
    data: object
    shapes: object
    tree: object


def road_asset(identity, keys, rows, data):
    n = len(keys)
    if (
        not 1 <= n <= 32
        or len(set(keys)) != n
        or len(data["body_faces"]) > 32768
        or data["owners"].shape != (len(data["top_faces"]), n)
        or data["owners"].dtype.kind != "b"
        or not data["owners"].any(axis=1).all()
        or data["ground"].shape != (len(data["top_faces"]),)
    ):
        raise ValueError("bounded closed asset and local source owners required")
    data = MappingProxyType({key: readonly(value) for key, value in data.items()})
    shapes = shapely.polygons(data["body_vertices"][data["top_faces"], :2])
    return RoadAsset(
        identity, tuple(keys), tuple(rows), data, shapes, shapely.STRtree(shapes)
    )


def load(output):
    output = Path(output)
    manifest = json.loads((output / "resident.json").read_text())
    if manifest["version"] != 1 or not 1 <= len(manifest["roads"]) <= 1024:
        raise ValueError("bounded resident research manifest required")
    terrain = Path(manifest["terrain"])
    if digest(terrain / "patches.json") != manifest["terrain_sha256"]:
        raise ValueError("original resident terrain manifest changed")
    view, terrain_manifest = load_prepared(terrain)
    if manifest["frame"] != view.patches[0].frame.identifier:
        raise ValueError("resident road and terrain frames differ")
    assets, keys = [], []
    for row in manifest["roads"]:
        path = (output / row["file"]).resolve()
        if not path.is_relative_to(output.resolve()) or digest(path) != row["sha256"]:
            raise ValueError("resident road payload changed")
        asset = road_asset(
            row["sha256"], row["source_ids"], row["rows"], read_arrays(path)
        )
        assets.append(asset)
        keys.extend(asset.source_ids)
    if (
        len(set(keys)) != len(keys)
        or keys != terrain_manifest["source_ids"]
        or keys != manifest["source_ids"]
        or manifest["origin_lat_lon"] != terrain_manifest["origin_lat_lon"]
    ):
        raise ValueError("independent road assets and terrain source ledger differ")
    support = Path(manifest["support_mesh"])
    if digest(support) != manifest["support_sha256"]:
        raise ValueError("resident whole support geometry changed")
    piers, contacts, _arrays = unpack(support)
    catalog = Path(manifest["catalog"])
    if digest(catalog) != manifest["catalog_sha256"]:
        raise ValueError("resident original source catalogue changed")
    return view, tuple(assets), piers, contacts, manifest


def road_window(assets, region, *, max_assets=16, max_sources=64, max_faces=32768):
    if any(
        type(value) is not int or not 1 <= value <= limit
        for value, limit in ((max_assets, 16), (max_sources, 64), (max_faces, 32768))
    ):
        raise ValueError("bounded local asset/source/body capacities required")
    chosen = [
        asset
        for asset in assets
        if len(asset.tree.query(region, predicate="intersects"))
    ]
    keys = [key for asset in chosen for key in asset.source_ids]
    if not 1 <= len(chosen) <= max_assets or not 1 <= len(keys) <= max_sources:
        raise ValueError("local road asset/source working budget exhausted")
    if sum(len(a.data["body_faces"]) for a in chosen) > max_faces:
        raise ValueError("local road body face working budget exhausted")
    columns, vertices, faces, top, bottom, ground = [], [], [], [], [], []
    offset, first = 0, 0
    for asset in chosen:
        data = asset.data
        vertices.append(data["body_vertices"])
        faces.append(data["body_faces"] + offset)
        top.append(data["top_faces"] + offset)
        bottom.append(data["bottom_faces"] + offset)
        columns.append(
            data["owners"].astype(np.uint64)
            @ (
                np.uint64(1)
                << np.arange(first, first + len(asset.source_ids), dtype=np.uint64)
            )
        )
        ground.append(data["ground"])
        offset += len(data["body_vertices"])
        first += len(asset.source_ids)
    road = {
        "body_vertices": np.vstack(vertices),
        "body_faces": np.vstack(faces),
        "top_faces": np.vstack(top),
        "bottom_faces": np.vstack(bottom),
        "face_sources": np.concatenate(columns),
    }
    return (
        road,
        np.concatenate(ground),
        keys,
        {r["source_id"]: r for a in chosen for r in a.rows},
        {
            "resident_assets_tested": len(assets),
            "local_assets": [a.identity for a in chosen],
            "local_sources": len(keys),
            "local_body_faces": len(road["body_faces"]),
            "local_road_array_bytes": sum(v.nbytes for v in road.values()),
            "source_mask_scope": "Bounded temporary local window only; resident owners are asset-local.",
        },
    )
