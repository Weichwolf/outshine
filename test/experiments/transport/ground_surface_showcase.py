"""Whole Vienna and clipped contact detail: independent DEM versus common surface."""

import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
import shapely
from capture_height_targets import sample_targets
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler, conform_triangles
from outshine_ref.transport import close_surface
from surface_solid_showcase import write_case
from transport_vertex_pool import welded_vertices

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUTPUT = ROOT / "build/experiments/transport/ground"
HEIGHTS = ROOT / "build/experiments/transport/heights"


def baseline_ground(bounds, meta):
    lo, hi = bounds
    axis = [
        np.linspace(lo[i], hi[i], int(np.ceil((hi[i] - lo[i]) / 25)) + 1)
        for i in range(2)
    ]
    x, y = np.meshgrid(*axis)
    xy = np.c_[x.ravel(), y.ravel()]
    h, _ = sample_targets(
        meta["origin_lat_lon"],
        xy,
        Path(meta["dem_sources"][0]["path"]).parent,
        meta["zoom"],
    )
    nx = len(axis[0])
    a = (np.arange(len(axis[1]) - 1)[:, None] * nx + np.arange(nx - 1)).ravel()
    faces = np.vstack((np.c_[a, a + 1, a + nx + 1], np.c_[a, a + nx + 1, a + nx]))
    return Mesh(np.c_[xy, h], faces)


def sample_vertices(sampler, xy, *, batch_queries=4096, max_candidates=1000000):
    if type(batch_queries) is not int or not 0 < batch_queries <= 65536:
        raise ValueError(
            "positive query batch within the existing sampler limit required"
        )
    points = np.empty((len(xy), 3))
    remaining = max_candidates
    for begin in range(0, len(xy), batch_queries):
        stop = min(len(xy), begin + batch_queries)
        contact = sampler.sample(
            xy[begin:stop], max_queries=batch_queries, max_candidates=remaining
        )
        if any(contact.reasons):
            reasons = sorted({reason for reason in contact.reasons if reason})
            raise ValueError("clipped contact unresolved: " + ", ".join(reasons))
        remaining -= contact.report["candidates"]
        points[begin:stop] = contact.points_enu_m
    return points


def clipped(mesh, slots, region, *, max_faces=65536):
    polygons = shapely.polygons(mesh.vertices[mesh.triangles, :2])
    hits = shapely.STRtree(polygons).query(region, predicate="intersects")
    pieces = shapely.intersection(polygons[hits], region)
    triangles, owners = shapely.get_parts(
        shapely.constrained_delaunay_triangles(pieces), return_index=True
    )
    points = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
    xy, labels = np.unique(points.reshape(-1, 2), axis=0, return_inverse=True)
    xy, canonical, displacement = welded_vertices(xy, 1e-8)
    faces = canonical[labels].reshape(-1, 3)
    points = sample_vertices(SurfaceSampler(mesh, max_faces=max_faces), xy)
    # Conformance must not silently change the owner mapping used for colours.
    indices, audit = conform_triangles(xy, faces)
    if audit["added_triangles"]:
        raise ValueError("inspection clipping needs additional interface ownership")
    return Mesh(points, indices), slots[hits[owners]], displacement


def body(mesh, slots, depth):
    result = close_surface(mesh, depth)
    materials = np.full(len(result.triangles), 2, dtype=int)
    materials[: len(mesh.triangles)] = slots
    return result, materials


def combined(items):
    vertices, faces, materials = [], [], []
    offset = 0
    for mesh, slots in items:
        vertices.append(mesh.vertices)
        faces.append(mesh.triangles + offset)
        materials.append(slots)
        offset += len(mesh.vertices)
    return Mesh(np.vstack(vertices), np.vstack(faces)), np.concatenate(materials)


def main():
    with np.load(OUTPUT / "Vienna-ground.npz") as data:
        ground = Mesh(data["vertices"], data["faces"])
        road_count = int(data["road_face_count"])
        vertex_map = data["road_vertex_map"]
    with np.load(HEIGHTS / "Vienna-cone-fit.npz") as data:
        road = Mesh(np.c_[data["xy"], data["heights"]], data["faces"])
    with np.load(
        ROOT
        / "build/experiments/transport/surface-vienna-cuts/Vienna-flat-surfaces.npz"
    ) as data:
        road_slots = np.where(data["part_roles"][data["face_owners"]], 1, 0)
    if not np.array_equal(ground.vertices[vertex_map, :2], road.vertices[:, :2]):
        raise ValueError("common ground changed road source coordinates")
    meta = json.loads((HEIGHTS / "Vienna-targets.json").read_text())
    terrain = baseline_ground(
        (ground.vertices[:, :2].min(axis=0), ground.vertices[:, :2].max(axis=0)), meta
    )
    terrain_slots = np.full(len(terrain.triangles), 5, dtype=int)
    ground_slots = np.r_[road_slots, np.full(len(ground.triangles) - road_count, 5)]
    samples = SurfaceSampler(terrain).sample(road.vertices[:, :2])
    difference = samples.points_enu_m[:, 2] - road.vertices[:, 2]
    if any(samples.reasons):
        raise ValueError("baseline grid does not cover the retained roads")
    centre = road.vertices[int(np.argmax(difference)), :2]
    region = shapely.box(*(centre - 60), *(centre + 60))
    contact = SurfaceSampler(ground).sample(ground.vertices[vertex_map, :2])
    mismatch = float(
        np.max(abs(contact.points_enu_m[:, 2] - ground.vertices[vertex_map, 2]))
    )
    if any(contact.reasons) or mismatch > 1e-7:
        raise ValueError("final road/ground contacts disagree")
    report = json.loads((OUTPUT / "Vienna-ground.json").read_text())
    report.update(
        baseline_buried_road_vertices=int(np.count_nonzero(difference > 1e-7)),
        baseline_maximum_above_road_m=float(difference.max()),
        maximum_contact_error_m=mismatch,
        soil_colour="olive",
        renderer_geometry="Composite ground top includes traffic once; 5 m display slab, not excavated earth volume.",
    )
    configurations = []
    for detail in (False, True):
        a, b, c = (
            (road, road_slots, 0.0),
            (terrain, terrain_slots, 0.0),
            (ground, ground_slots, 0.0),
        )
        if detail:
            a, b, c = (
                clipped(road, road_slots, region),
                clipped(terrain, terrain_slots, region),
                clipped(ground, ground_slots, region),
            )
        before, before_slots = combined((body(a[0], a[1], 0.25), body(b[0], b[1], 5.0)))
        after, after_slots = body(c[0], c[1], 5.0)
        name = "Vienna-ground-detail" if detail else "Vienna-ground"
        for suffix, mesh, slots in (
            ("before", before, before_slots),
            ("after", after, after_slots),
        ):
            record = dict(
                report,
                valid_surface=suffix == "after",
                diagnostic_interpenetration=suffix == "before",
                buried_road_vertices=report["baseline_buried_road_vertices"]
                if suffix == "before"
                else 0,
                display_depth_m=5.0,
                detail_region_xy=list(region.bounds) if detail else None,
                maximum_inspection_weld_m=max(a[2], b[2], c[2]),
            )
            write_case(
                name + "-" + suffix,
                mesh,
                slots,
                record,
                configurations,
                output_dir=OUTPUT,
            )
        for key in ("eye", "target", "span_m", "profile"):
            configurations[-1][key] = configurations[-2][key]
    scene_path = OUTPUT / "scenes.json"
    scene_path.write_text(json.dumps(configurations, indent=2) + "\n")
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
            str(scene_path),
        ],
        cwd=ROOT,
        check=True,
    )
    write_origin(configurations)


def write_origin(configurations):
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
    ).strip()
    dirty = subprocess.check_output(
        ["git", "status", "--porcelain", "--", "lib", "test/experiments/transport"],
        cwd=ROOT,
        text=True,
    ).strip()
    records = {}
    for configuration in configurations:
        base = OUTPUT / f"{configuration['id']}-solid"
        records[base.name] = {
            "image_sha256": hashlib.sha256(
                base.with_suffix(".png").read_bytes()
            ).hexdigest(),
            "report_sha256": hashlib.sha256(
                base.with_suffix(".json").read_bytes()
            ).hexdigest(),
        }
    (OUTPUT / "origin.json").write_text(
        json.dumps(
            {"commit": commit, "working_tree_dirty": bool(dirty), "records": records},
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
