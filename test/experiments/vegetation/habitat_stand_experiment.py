"""Real source stand parents and one requested near cell, without tree construction."""

import argparse
import json
import time
from pathlib import Path

import numpy as np
import shapely
from outshine_ref import LocalFrame
from outshine_ref.geometry import projected_height_surface
from outshine_ref.transport import close_surface
from outshine_ref.vegetation import HabitatStand, StandHabitat

ROOT = Path(__file__).resolve().parents[3]
CASES = ROOT / "lib/tests/data/habitat-stand-cases.geojson"


def plans(path=CASES):
    for feature in json.loads(path.read_text())["features"]:
        row = feature["properties"]
        habitat = StandHabitat.prepare(
            LocalFrame(row["frame"]),
            row["source_id"],
            row["region_id"],
            shapely.geometry.shape(feature["geometry"]),
            tuple(row["region_lower_xy_m"]),
            row["region_side_m"],
        )
        yield (
            feature,
            HabitatStand(
                habitat,
                tuple(row["kinds"]),
                tuple(row["counts"]),
                tuple(row["heights_m"]),
                tuple(row["spreads_m"]),
                levels=row["levels"],
                seed=row["seed"],
            ),
        )


def support_bodies(fields):
    vertices, faces, normals, keys = [], [], [], []
    offset = 0
    for field in fields:
        if not int(field["counts"].sum()):
            continue
        domain = field["domain"]
        height = field["mean_height_m"]
        lower, _, upper, _ = domain.bounds
        top, report = projected_height_surface(
            domain, (0, 0), (1, 0), [(lower - 1, height), (upper + 1, height)]
        )
        if top is None:
            raise ValueError(str(report))
        body = close_surface(top, height)
        p = body.vertices[body.triangles]
        n = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
        n /= np.linalg.norm(n, axis=1, keepdims=True)
        vertices.append(body.vertices)
        faces.append(body.triangles + offset)
        normals.append(np.repeat(n, 3, axis=0))
        keys.append(np.tile(field["cell"], (len(body.triangles), 1)))
        offset += len(body.vertices)
    return {
        "vertices": np.concatenate(vertices),
        "faces": np.concatenate(faces).astype(np.int32),
        "normals": np.concatenate(normals),
        "cell_keys": np.concatenate(keys).astype(np.int32),
    }


def run(output):
    output.mkdir(parents=True, exist_ok=False)
    reports = []
    for feature, plan in plans():
        name = str(feature["properties"]["feature_id"])
        folder = output / name
        folder.mkdir()
        start = time.perf_counter()
        root = plan.field()
        root_cells = len(plan._cells)
        assert len(plan._cells) == 1 and not plan._sites
        fields = [plan.field((3, x, y)) for x in range(8) for y in range(8)]
        np.testing.assert_array_equal(
            np.sum([f["counts"] for f in fields], axis=0), root["counts"]
        )
        assert not plan._sites
        built = time.perf_counter()
        coarse_cells = len(plan._cells)
        body = support_bodies(fields)
        np.savez_compressed(folder / "coarse-support.npz", **body)
        np.savez_compressed(
            folder / "coarse-fields.npz",
            keys=np.array([f["cell"] for f in fields], np.int32),
            counts=np.array([f["counts"] for f in fields], np.uint64),
            area_m2=np.array([f["area_m2"] for f in fields]),
            height_m=np.array([f["mean_height_m"] for f in fields]),
            cover_estimate=np.array([f["cover_estimate"] for f in fields]),
        )
        (folder / "coarse-plan.json").write_text(
            json.dumps(plan.recipe(), indent=2) + "\n"
        )
        coarse_ready = time.perf_counter()
        key = (0, 0, 0)
        for _ in range(plan.levels):
            key = max(plan.children(key), key=lambda c: (sum(c.counts), c.key)).key
        sites = plan.sites(key)
        np.savez_compressed(folder / "near-sites.npz", **sites)
        assert len(plan._sites) == 1
        (folder / "detail-plan.json").write_text(
            json.dumps(plan.recipe(), indent=2) + "\n"
        )
        (folder / "source.json").write_text(json.dumps(feature, indent=2) + "\n")
        report = {
            "source_id": plan.habitat.source_id,
            "region_id": plan.habitat.region_id,
            "frame": plan.habitat.frame.identifier,
            "layout_identity": plan.layout_identity,
            "identity": plan.identity,
            "counts": list(plan.counts),
            "root_area_m2": root["area_m2"],
            "root_only_cells": root_cells,
            "coarse_only_cells": coarse_cells,
            "coarse_sites": 0,
            "coarse_field_s": built - start,
            "coarse_with_preview_s": coarse_ready - start,
            "coarse_triangles": len(body["faces"]),
            "coarse_array_bytes": sum(a.nbytes for a in body.values()),
            "near_key": list(key),
            "near_sites": len(sites["ids"]),
            "near_payload_bytes": sum(a.nbytes for a in sites.values()),
            "prepared_cells": len(plan._cells),
            "geometry_wkb_payload_bytes": plan._geometry_bytes,
            "retained_sites": plan._site_count,
            "total_s": time.perf_counter() - start,
            "scope": "Real source support and integer parent measure. Closed support-body heights are diagnostic descriptors, not approved canopy geometry/coverage/radiance. XY roots only; terrain, source acquisition, reservations and native products remain caller-owned. No individual tree or organ generation.",
        }
        (folder / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
        reports.append(report)
    (output / "summary.json").write_text(json.dumps(reports, indent=2) + "\n")
    print(json.dumps(reports), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
