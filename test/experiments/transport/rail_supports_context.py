"""Actual elevated-deck/held-world bindings and conservative foreign traffic masks."""

import json
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from elevated_rail_audit import inspect
from elevated_rail_inputs import read_sources
from elevated_rail_study import old_arrays
from outshine_ref.contracts import Mesh
from outshine_ref.transport.supports import SupportVolumeBuilder
from stair_local_context import radius
from stair_local_replay import load_chain
from stair_module_inputs import FIXTURE
from stair_support_audit import Reference
from stair_support_traffic import RegionMask


@dataclass(frozen=True)
class SupportContext:
    lower: Mesh
    upper: Mesh
    builder: SupportVolumeBuilder | None
    lower_reference: Reference
    upper_reference: Reference
    exclusions: tuple
    work: dict


def foreign_masks(domain, assets, piers):
    chosen = []
    for asset in assets:
        hits = asset.tree.query(
            domain, predicate="dwithin", distance=0.2 + asset.xy_rounding_m
        )
        if len(hits):
            chosen.append((asset, hits))
    if (
        len(chosen) > 16
        or sum(len(a.source_ids) for a, _ in chosen) > 64
        or sum(len(a.data["body_faces"]) for a, _ in chosen) > 32768
        or len(piers) > 128
    ):
        raise ValueError(
            "Bounded old support traffic asset/source/body context required"
        )
    masks, counts = [], []
    for cast in (False, True):
        parts = []
        for asset, hits in chosen:
            xyz = asset.data["body_vertices"]
            if cast:
                xyz = xyz.astype("f4").astype(float)
            parts.extend(shapely.polygons(xyz[asset.data["top_faces"][hits], :2]))
        used_piers = 0
        for pier in piers:
            xyz = pier.vertices.astype("f4").astype(float) if cast else pier.vertices
            shape = shapely.convex_hull(shapely.multipoints(xyz[:, :2]))
            if shapely.dwithin(shape, domain, 0.2):
                parts.append(shape)
                used_piers += 1
        counts.append(
            {
                "float32": cast,
                "traffic_faces": sum(len(hits) for _, hits in chosen),
                "old_pier_bounds": used_piers,
            }
        )
        masks.append(RegionMask(shapely.union_all(parts)))
    return tuple(masks), {
        "asset_metadata_queries": len(assets),
        "selected_assets": len(chosen),
        "selected_source_owners": sum(len(a.source_ids) for a, _ in chosen),
        "selected_body_faces": sum(len(a.data["body_faces"]) for a, _ in chosen),
        "old_support_bounds_tested": len(piers),
        "masks": counts,
    }


DEFAULT_FIXTURE = FIXTURE.with_name("stair-elevated-rail-vienna.json")


def load(study, fixture=DEFAULT_FIXTURE, *, construction=True):
    report = json.loads((study / "report.json").read_text())
    path = study / "candidates.npz"
    if (
        report["selected_method"] != "flat"
        or not report["methods"]["flat"]["accepted"]
        or digest(path) != report["candidates_sha256"]
        or digest(fixture) != report["fixture_sha256"]
    ):
        raise ValueError("Exact admitted flat deck candidate/fixture required")

    view, assets, piers, _contacts, manifest = load_chain(
        Path(report["resident"]), [Path(p["path"]) for p in report["prior_modules"]]
    )
    if (
        manifest["source_ids"] != report["old_source_ids"]
        or old_arrays(assets) != report["old_road_arrays"]
    ):
        raise ValueError("Actual complete preceding source/road arrays changed")
    recipe, context, _available, rows = read_sources(fixture, manifest["source_ids"])
    if (
        context["origin_lat_lon"] != report["origin_lat_lon"]
        or report["origin_lat_lon"] != manifest["origin_lat_lon"]
    ):
        raise ValueError("Deck/source/held-world frames differ")
    if report["source_ids"] != [r["source_id"] for r in rows] or report[
        "original_properties"
    ] != [r["original_properties"] for r in rows]:
        raise ValueError("Deck original source/role bindings differ")
    with np.load(path, allow_pickle=False) as saved:
        body = Mesh(saved["flat_vertices"], saved["flat_triangles"])
    n, m = report["whole_layout"]["vertices"], report["whole_layout"]["faces"]
    coefficients = report["methods"]["flat"]["fit"]["coefficients"]
    if coefficients[:2] != [0, 0] or not np.all(
        body.vertices[:n, 2] == coefficients[2]
    ):
        raise ValueError("Exact retained constant rail deck required")
    inspect(
        body,
        body.triangles[:m],
        rows,
        [],
        recipe["deck_thickness_m"],
        recipe["along_grade"],
        recipe["cross_grade"],
    )
    upper = Mesh(body.vertices[n:], body.triangles[:m])
    domain = shapely.union_all(shapely.polygons(upper.vertices[upper.triangles, :2]))
    query = view.nearby(domain, radius(view), max_faces=8192)
    if query.mesh is None:
        raise ValueError("Whole support terrain context absent: " + query.reason)
    masks, work = foreign_masks(domain, assets, piers)
    builder = SupportVolumeBuilder(query.mesh, upper) if construction else None
    result = SupportContext(
        query.mesh,
        upper,
        builder,
        Reference(query.mesh),
        Reference(upper),
        masks,
        {
            **work,
            "terrain_faces": len(query.mesh.triangles),
            "upper_faces": len(upper.triangles),
            "builder_preparation_s": builder.preparation_seconds
            if builder is not None
            else 0,
        },
    )
    return result, rows, report
