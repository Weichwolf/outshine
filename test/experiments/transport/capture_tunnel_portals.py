"""Explicit offline capture of two subway tunnel sources and chosen floor contacts."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from capture_height_targets import sample_targets
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
BASE = ROOT / "build/experiments/transport"
MODEL = BASE / "bridge-continuations/Vienna-continuation-whole-rebuild-solid.npz"
FIXTURE = HERE / "fixtures/tunnel-portals-vienna.json"
PIN = "f7b728c98e21bda79beb38cceea0e8152361af64451e11a73a8e4267b6793c56"
IDS = (
    "openfreemap:14/8938/5680:transportation:2154579170:0",
    "openfreemap:14/8938/5680:transportation:2154579170:1",
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def base_surfaces(path=MODEL):
    report = json.loads(path.with_suffix(".json").read_text())
    origin = json.loads((path.parent / "origin.json").read_text())
    if origin["working_tree_dirty"] or report["mesh_sha256"] != digest(path):
        raise ValueError("clean hash-checked base geometry required")
    if origin["records"][path.stem]["report_sha256"] != digest(
        path.with_suffix(".json")
    ):
        raise ValueError("base report changed")
    with np.load(path) as data:
        vertices, faces, slots = data["vertices"], data["triangles"], data["materials"]
    xy = vertices[faces, :2]
    a, b = xy[:, 1] - xy[:, 0], xy[:, 2] - xy[:, 0]
    positive = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0] > 0
    meshes = []
    for mask in (positive & np.isin(slots, (1, 3)), positive & (slots == 0)):
        used, inverse = np.unique(faces[mask], return_inverse=True)
        meshes.append(Mesh(vertices[used], inverse.reshape(-1, 3)))
    return tuple(meshes), {
        "commit": origin["commit"],
        "mesh_sha256": digest(path),
        "report_sha256": digest(path.with_suffix(".json")),
    }


def terrain_model(xy):
    lo, hi = xy.min(axis=0) - 25, xy.max(axis=0) + 25
    axes = [
        np.linspace(lo[i], hi[i], int(np.ceil((hi[i] - lo[i]) / 25)) + 1)
        for i in range(2)
    ]
    if len(axes[0]) * len(axes[1]) > 4096:
        raise ValueError("terrain capture vertex budget")
    x, y = np.meshgrid(*axes)
    points = np.c_[x.ravel(), y.ravel()]
    cache = (
        Path.home()
        / ".local/share/outshine/outshine/sources/transport-experiments/mapterhorn"
    )
    heights, receipts = sample_targets([48.21, 16.37], points, cache, 11, False)
    nx = len(axes[0])
    first = (np.arange(len(axes[1]) - 1)[:, None] * nx + np.arange(nx - 1)).ravel()
    faces = np.vstack(
        (
            np.c_[first, first + 1, first + nx + 1],
            np.c_[first, first + nx + 1, first + nx],
        )
    )
    return {
        "vertices": np.c_[points, heights].tolist(),
        "triangles": faces.tolist(),
        "maximum_grid_step_m": 25.0,
        "receipts": [
            {k: v for k, v in r.items() if k != "acquisition_and_decode_ms"}
            for r in receipts
        ],
        "scope": "Selected coarse C0 ground reference from pinned DEM; no continuous DEM error bound or survey reconstruction.",
    }


def capture():
    path = BASE / "inputs/Vienna-Donau-West-input.json"
    raw = json.loads(path.read_text())
    if raw["origin_lat_lon"] != [48.21, 16.37] or len(raw["roads"]) > 4096:
        raise ValueError("bounded common source frame required")
    if digest(Path(raw["receipt"]["path"])) != raw["receipt"]["sha256"]:
        raise ValueError("pinned vector source changed")
    bridge_sources = json.loads(
        (HERE / "fixtures/bridge-continuations-vienna.json").read_text()
    )["sources"]
    ground = [s for s in bridge_sources if s["role"] == "approach_ground"]
    meshes, base = base_surfaces()
    sampler = SurfaceSampler(meshes[0])
    chosen = []
    for identity in IDS:
        source = next(s for s in raw["roads"] if s["source_id"] == identity)
        if source["properties"] != dict(
            subclass="subway", brunnel="tunnel", **{"class": "transit", "layer": -1}
        ):
            raise ValueError("expected explicit subway tunnel source required")
        xy = np.asarray(source["coordinates"])
        if not 2 <= len(xy) <= 256:
            raise ValueError("tunnel source coordinate budget")
        matches = [
            (i, g)
            for i in (0, len(xy) - 1)
            for g in ground
            if np.min(np.linalg.norm(np.asarray(g["xy"])[[0, -1]] - xy[i], axis=1))
            < 1e-6
        ]
        if len(matches) != 1:
            raise ValueError("one explicit ground source endpoint contact required")
        index, contact_source = matches[0]
        result = sampler.sample(xy[[index]])
        if any(result.reasons) or np.linalg.norm(result.normals_enu[0, :2]) > 1e-10:
            raise ValueError("flat chosen base endpoint surface required")
        chosen.append(
            {
                "source_id": identity,
                "xy": xy.tolist(),
                "width_m": source["width_m"] if source["width_m"] is not None else 4.0,
                "width_assumed": True,
                "width_origin": "class_recipe"
                if source["width_m"] is not None
                else "explicit_subway_recipe_4m",
                "properties": source["properties"],
                "original": source,
                "contact": {
                    "source_id": contact_source["source_id"],
                    "endpoint_index": index,
                    "xyz_m": result.points_enu_m[0].tolist(),
                    "kind": "planned_ground_rail_surface",
                },
            }
        )
    heights = [s["contact"]["xyz_m"][2] for s in chosen]
    if abs(heights[0] - heights[1]) > 1e-7:
        raise ValueError("explicit shared portal floor required")
    return {
        "sources": chosen,
        "base": base,
        "origin_lat_lon": raw["origin_lat_lon"],
        "input": {
            "path": str(path.relative_to(ROOT)),
            "sha256": digest(path),
            "receipt": raw["receipt"],
        },
        "terrain": terrain_model(np.vstack([s["xy"] for s in chosen])),
        "scope": "Real pinned tunnel axes with explicitly chosen existing ground contacts; far clips are not ground anchors. Tunnel depth, cover, clear section and portal position remain own procedural choices.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write-fixture", action="store_true")
    args = parser.parse_args()
    if not args.write_fixture:
        parser.error("explicit --write-fixture required; normal tests only read pins")
    FIXTURE.write_text(json.dumps(capture(), indent=2) + "\n")
    print(digest(FIXTURE))


if __name__ == "__main__":
    main()
