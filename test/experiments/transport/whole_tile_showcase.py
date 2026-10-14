"""Four progress snapshots framed by complete, pinned OpenFreeMap source tiles."""

import hashlib
import json
import math
import subprocess
from pathlib import Path

import numpy as np
import shapely
from ground_surface_showcase import body, clipped, combined
from outshine_ref.contracts import Mesh
from PIL import Image
from retained_surface import surface_domain
from surface_solid_showcase import inspection_view, write_case

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
BASE = ROOT / "build/experiments/transport"
OUTPUT = BASE / "whole-tiles"
GIRTH_M = 40075016.68557849
COLOURS = (
    (0.14, 0.20, 0.20, 1.0),
    (0.08, 0.42, 0.29, 1.0),
    (0.50, 0.57, 0.53, 1.0),
    (0.12, 0.43, 0.65, 1.0),
    (0.9, 0.29, 0.1, 1.0),
    (0.67, 0.70, 0.54, 1.0),
    (0.48, 0.52, 0.47, 1.0),
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tile_region(name):
    path = BASE / "inputs" / f"{name}-input.json"
    source = json.loads(path.read_text())
    receipt = source["receipt"]
    if digest(Path(receipt["path"])) != receipt["sha256"]:
        raise ValueError("pinned OpenFreeMap source changed")
    latitude, longitude = source["origin_lat_lon"]
    zoom, tx, ty = receipt["tile"]
    span = GIRTH_M / (1 << zoom)
    anchor = np.array(
        [
            longitude / 360 * GIRTH_M,
            math.asinh(math.tan(math.radians(latitude))) * GIRTH_M / (2 * math.pi),
        ]
    )
    # Evaluate shared integer tile edges identically, without accumulated span error.
    corners = np.array(
        [
            [x * span - GIRTH_M / 2, GIRTH_M / 2 - y * span]
            for x, y in ((tx, ty + 1), (tx + 1, ty + 1), (tx + 1, ty), (tx, ty))
        ]
    )
    corners = (corners - anchor) * math.cos(math.radians(latitude))
    region = shapely.Polygon(corners)
    proof = {
        "input": str(path.relative_to(ROOT)),
        "input_sha256": digest(path),
        "receipt": receipt,
        "origin_lat_lon": source["origin_lat_lon"],
        "tile_boundary_xy_m": corners.tolist(),
    }
    return region, proof


def snapshot(stem, *, diagnostic=False):
    path = BASE / f"{stem}.npz"
    report_path = path.with_suffix(".json")
    report = json.loads(report_path.read_text())
    origin = json.loads((path.parent / "origin.json").read_text())
    if origin["working_tree_dirty"] or not origin["commit"]:
        raise ValueError("snapshot needs a clean generation commit")
    if digest(report_path) != origin["records"][path.stem]["report_sha256"]:
        raise ValueError("snapshot report changed")
    if digest(path) != report["mesh_sha256"]:
        raise ValueError("snapshot geometry changed")
    if report.get("valid") is False:
        raise ValueError("showcase snapshot must be valid")
    if report.get("valid_surface") is False and not (
        diagnostic and report.get("diagnostic_interpenetration") is True
    ):
        raise ValueError(
            "invalid surface is only allowed as the explicit before diagnostic"
        )
    with np.load(path) as data:
        mesh = Mesh(data["vertices"], data["triangles"])
        slots = data["materials"]
    provenance = {
        "generator_commit": origin["commit"],
        "mesh_source": stem,
        "mesh_source_sha256": digest(path),
        "report_source_sha256": digest(report_path),
        "generator_scope": report.get("scope", report.get("display_scope", "")),
        "diagnostic_interpenetration": report.get("diagnostic_interpenetration", False),
    }
    return mesh, slots, provenance


def crop_surfaces(mesh, slots, region, groups):
    xy = mesh.vertices[mesh.triangles, :2]
    a, b = xy[:, 1] - xy[:, 0], xy[:, 2] - xy[:, 0]
    positive = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0] > 0
    assignments = sum(np.isin(slots, group["slots"]).astype(int) for group in groups)
    if np.any(assignments[positive] != 1):
        raise ValueError(
            "each source top face must belong to exactly one displayed surface"
        )
    parts, checks = [], []
    for group in groups:
        selected = positive & np.isin(slots, group["slots"])
        depth = group["depth_m"]
        used, indices = np.unique(mesh.triangles[selected], return_inverse=True)
        original = Mesh(mesh.vertices[used], indices.reshape(-1, 3))
        result, colours, weld = clipped(original, slots[selected], region)
        expected = shapely.intersection(surface_domain(original), region)
        coverage = shapely.symmetric_difference(surface_domain(result), expected).area
        if coverage > 1e-5:
            raise ValueError("tile clipping changed surface coverage")
        parts.append(body(result, colours, depth))
        checks.append(
            {
                "coverage_error_m2": coverage,
                "maximum_clipping_weld_m": weld,
                "displayed_faces": len(result.triangles),
                "body_depth_m": depth,
            }
        )
    return parts, checks


def tile_frame(regions, height):
    outline = shapely.union_all([region.boundary.buffer(1.0) for region in regions])
    faces = shapely.get_parts(shapely.constrained_delaunay_triangles(outline))
    xy = shapely.get_coordinates(faces).reshape(-1, 4, 2)[:, :3]
    xy, labels = np.unique(xy.reshape(-1, 2), axis=0, return_inverse=True)
    mesh = Mesh(np.c_[xy, np.full(len(xy), height)], labels.reshape(-1, 3))
    return body(mesh, np.full(len(mesh.triangles), 6, dtype=int), 0.1)


def prepare(entry, configurations):
    tiles = [tile_region(name) for name in entry["tiles"]]
    regions = [tile[0] for tile in tiles]
    region = shapely.union_all(regions)
    original, slots, provenance = snapshot(
        entry["model"], diagnostic=entry.get("diagnostic", False)
    )
    parts, checks = crop_surfaces(original, slots, region, entry["surface_groups"])
    parts.append(tile_frame(regions, original.vertices[:, 2].min() - 2.0))
    model, materials = combined(parts)
    view = inspection_view(model.vertices, eye_direction=(-0.3, -1.0, 0.85))
    view["colours"] = COLOURS
    report = {
        "scope": entry["scope"],
        "snapshot": provenance,
        "tiles": [tile[1] for tile in tiles],
        "clipping": checks,
        "full_source_tiles": True,
        "height_exaggeration": 1.0,
        "tile_frame_display_only": True,
        "annotations": "Thin tile outlines at a lower height are display guides, not terrain or generator assets.",
    }
    write_case(
        entry["id"],
        model,
        materials,
        report,
        configurations,
        view=view,
        output_dir=OUTPUT,
    )
    return model


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    entries = json.loads((HERE / "showcase_catalog.json").read_text())["images"]
    if len(entries) != 4:
        raise ValueError("whole-tile showcase needs exactly four entries")
    configurations = []
    comparisons = {}
    for entry in entries:
        model = prepare(entry, configurations)
        comparisons.setdefault(entry["comparison"], []).append(
            (model, configurations[-1])
        )
    for pair in comparisons.values():
        if len(pair) != 2:
            raise ValueError(
                "each comparison needs the complete before and after tile views"
            )
        view = inspection_view(
            np.vstack([item[0].vertices for item in pair]),
            eye_direction=(-0.3, -1.0, 0.85),
        )
        for _model, configuration in pair:
            configuration.update(view)
    scene = OUTPUT / "scenes.json"
    scene.write_text(json.dumps(configurations, indent=2) + "\n")
    # Old files cannot make an interrupted or incomplete render appear successful.
    for configuration in configurations:
        Path(configuration["output"]).unlink(missing_ok=True)
    subprocess.run(
        [
            "blender",
            "--background",
            "--factory-startup",
            "--python-exit-code",
            "1",
            "--python",
            str(HERE / "render_transport_solid.py"),
            "--",
            str(scene),
        ],
        cwd=ROOT,
        check=True,
    )
    records = {}
    for configuration in configurations:
        image = Path(configuration["output"])
        with Image.open(image) as picture:
            if picture.size != (1920, 1080) or picture.format != "PNG":
                raise ValueError("fresh complete 1920×1080 PNG required")
            picture.verify()
        records[image.stem] = {
            "image_sha256": digest(image),
            "report_sha256": digest(image.with_suffix(".json")),
        }
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
    ).strip()
    dirty = subprocess.check_output(
        [
            "git",
            "status",
            "--porcelain",
            "--",
            "lib",
            "test/experiments/transport",
        ],
        cwd=ROOT,
        text=True,
    ).strip()
    (OUTPUT / "origin.json").write_text(
        json.dumps(
            {
                "commit": commit,
                "working_tree_dirty": bool(dirty),
                "records": records,
                "scene_sha256": digest(scene),
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
