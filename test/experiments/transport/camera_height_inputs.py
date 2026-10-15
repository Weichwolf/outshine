"""Pinned whole-window DEM priors and complete fixed rail-bed facets."""

import hashlib
import json
from pathlib import Path

import numpy as np
import shapely
from capture_height_targets import sample_targets
from rail_graph_inputs import read_window
from surface_height_frames import frames_for_surface

HERE = Path(__file__).resolve().parent


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def rail_footprints(root, name, camera_generation):
    generation = json.loads((root / "generation.json").read_text())
    verification = json.loads((root / "verification.json").read_text())
    if (
        generation["dirty"]
        or verification["diagnostic_dirty_allowed"]
        or verification["generation_sha256"] != digest(root / "generation.json")
        or not verification["cases"][name]["valid"]
    ):
        raise ValueError("clean independently verified rail products required")
    row = generation["cases"][name]
    if row["pins"]["generation_sha256"] != camera_generation:
        raise ValueError("rail and bed camera generations differ")
    path = root / (name + ".npz")
    if digest(path) != row["mesh_sha256"]:
        raise ValueError("verified rail payload changed")
    with np.load(path) as data:
        vertices = data["vertices"]
    footprints, offset = [], 0
    for piece in row["pieces"]:
        if not piece["valid"]:
            continue
        nv, sleepers = piece["rail_vertices"], piece["sleepers"]
        rings = vertices[offset : offset + nv].reshape(2, -1, 12, 3)
        footprints.extend(
            shapely.Polygon(np.vstack((r[:, 0, :2], r[::-1, 1, :2]))) for r in rings
        )
        boxes = vertices[offset + nv : offset + nv + 8 * sleepers]
        footprints.extend(shapely.polygons(boxes.reshape(-1, 8, 3)[:, :4, :2]))
        offset += nv + 8 * sleepers
    if offset != len(vertices):
        raise ValueError("incomplete rail footprint reconstruction")
    return (
        np.array(footprints, object),
        row["horizontal_bed_datum_m"],
        {
            "generation_sha256": digest(root / "generation.json"),
            "verification_sha256": digest(root / "verification.json"),
            "mesh_sha256": row["mesh_sha256"],
        },
    )


def fixed_bed_facets(xy, faces, footprints, maximum_pairs=2000000):
    triangles = shapely.polygons(xy[faces])
    tree = shapely.STRtree(triangles)
    selected = np.zeros(len(faces), bool)
    covered = np.zeros(len(footprints), bool)
    count = 0
    for begin in range(0, len(footprints), 64):
        pairs = tree.query(footprints[begin : begin + 64], predicate="intersects")
        count += pairs.shape[1]
        if count > maximum_pairs:
            raise ValueError("fixed_rail_facet_pair_budget")
        selected[pairs[1]] = True
        # A hit alone does not prove footprint coverage. Check the complete union
        # of pinned facets below before treating these as physical contacts.
        covered[begin + pairs[0]] = True
    pinned = np.unique(faces[selected])
    domain = shapely.coverage_union_all(triangles[selected])
    shapely.prepare(domain)
    if not covered.all() or not shapely.covers(domain, footprints).all():
        raise ValueError("whole verified hardware footprint lacks fixed bed")
    return pinned, {
        "footprints": len(footprints),
        "fixed_facets": int(selected.sum()),
        "fixed_vertices": len(pinned),
        "intersection_candidates": count,
    }


def lumped_weights(xy, faces):
    p = xy[faces]
    a, b = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    areas = abs(a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]) / 2
    weights = np.bincount(
        faces.ravel(), weights=np.repeat(areas / 3, 3), minlength=len(xy)
    )
    if np.any(areas <= 0) or np.any(weights <= 0):
        raise ValueError("positive triangle areas and referenced vertices required")
    return weights * len(xy) / weights.sum()


def prepare_case(camera, rails, name, sources, output):
    source_name = "Vienna" if name == "Wien" else name
    original, _raw, _graphs, _domain, binding = read_window(camera, source_name)
    generation = json.loads((camera / "generation.json").read_text())["files"]
    prefix = f"results/{source_name}/components/"
    relative = prefix + "product-0.npz"
    path = camera / relative
    report_path = camera / (prefix + "result.json")
    if digest(report_path) != generation[prefix + "result.json"]:
        raise ValueError("camera result receipt changed")
    report = json.loads(report_path.read_text())
    if report["products"] != 1 or digest(path) != generation[relative]:
        raise ValueError("one complete pinned camera mesh required")
    with np.load(path) as data:
        xy, faces = data["vertices"], data["indices"]
        roles = data["part_roles"][data["face_owners"]]
    if len(xy) > 131072 or len(faces) > 160000:
        raise ValueError("camera_height_input_budget")
    ledger = json.loads((camera / (prefix + "source-usage.json")).read_text())
    used = {r["id"] for r in ledger if r["status"] != "not_used"}
    frames, frame_report = frames_for_surface(xy, faces, roles, original, used)
    targets, receipts = sample_targets(original["origin_lat_lon"], xy, sources, 11)
    recipes = json.loads((HERE / "reference/place-sources.json").read_text())
    expected = {
        r["url"]: r["sha256"]
        for c in recipes["cases"]
        if c["place"] == name
        for r in c["flat_recipe_dem"]
    }
    if any(expected.get(r["url"]) != r["sha256"] for r in receipts):
        raise ValueError("coarse DEM bytes differ from retained comparison source")
    shapes, datum, rail_binding = rail_footprints(
        rails, name, binding["generation_sha256"]
    )
    fixed, contacts = fixed_bed_facets(xy, faces, shapes)
    arrays = {
        "xy": xy,
        "faces": faces,
        "targets": targets,
        "frames": frames,
        "fixed": fixed,
        "weights": lumped_weights(xy, faces),
        "roles": roles,
    }
    np.savez_compressed(output / (name + "-input.npz"), **arrays)
    metadata = {
        "place": name,
        "source_binding": binding,
        "bed_sha256": digest(path),
        "source_usage_sha256": digest(camera / (prefix + "source-usage.json")),
        "rail_binding": rail_binding,
        "origin_lat_lon": original["origin_lat_lon"],
        "fixed_datum_m": datum,
        "contacts": contacts,
        "frames": frame_report,
        "dem_receipts": receipts,
        "input_sha256": digest(output / (name + "-input.npz")),
    }
    (output / (name + "-input.json")).write_text(json.dumps(metadata, indent=2) + "\n")
    return {"place": name, "vertices": len(xy), "faces": len(faces), **contacts}
