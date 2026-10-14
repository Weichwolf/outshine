"""Same-operation controls and saved traffic footprints against external SDK oracles."""

import hashlib
import json
from pathlib import Path
import subprocess
import time

import numpy as np
import shapely

from outshine_ref.geometry import conform_triangles
from esri_transport_oracle import reference


ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "build/experiments/transport/esri-reference"
INPUT = ROOT / "build/experiments/transport/inputs/Vienna-input.json"
SURFACES = ROOT / "build/experiments/transport/surface-vienna-cuts/Vienna-flat-surfaces.npz"


def controls():
    left, right = shapely.box(0, 0, 4, 4), shapely.box(8, 0, 12, 4)
    dumbbell = shapely.union_all([left, right, shapely.box(4, 1.75, 8, 2.25)])
    return [("rectangle", shapely.box(0, 0, 10, 6), 1.),
            ("concave-L", shapely.Polygon([(0, 0), (8, 0), (8, 3), (3, 3), (3, 8), (0, 8)]), .5),
            ("dumbbell", dumbbell, .4), ("collapsed", shapely.box(0, 0, 1, 1), .6),
            ("courtyard", shapely.Polygon([(0, 0), (10, 0), (10, 10), (0, 10)], [[(3, 3), (3, 7), (7, 7), (7, 3)]]), .5)]


def real_parts(limit=24):
    cases, skipped = [], []
    with np.load(SURFACES) as data:
        positions, owners, roles = data["vertices"][:, :2], data["face_owners"], data["part_roles"]
        polygons = shapely.polygons(positions[data["indices"]])
    identifiers = np.unique(owners)
    corridors, junctions = identifiers[roles[identifiers] == 0], identifiers[roles[identifiers] != 0]
    candidates = [owner for pair in zip(corridors, junctions) for owner in pair]
    for owner in candidates:
        polygon = shapely.coverage_union_all(polygons[owners == owner])
        vertices = len(shapely.get_coordinates(polygon))
        if polygon.geom_type != "Polygon" or not polygon.is_valid or vertices > 256:
            skipped.append(dict(part_id=int(owner), reason="bounded_single_polygon_reference_scope"))
            continue
        cases.append((f"Vienna-{'junction' if roles[owner] else 'corridor'}-{owner}", polygon, .2))
        if len(cases) == limit:
            break
    return cases, skipped


def offset_case(name, polygon, distance):
    if polygon.interiors:
        return dict(id=name, unavailable_reason="selected_offset_helper_has_no_explicit_hole_argument", input_wkt=polygon.wkt)
    xy = np.asarray(polygon.exterior.coords, dtype=np.float32)[:-1]
    quantized = shapely.Polygon(xy)
    if not quantized.is_valid:
        return dict(id=name, unavailable_reason="float32_input_ring_not_simple", input_wkt=polygon.wkt)
    record = reference(dict(operation="offset", xy=xy.tolist(), distance=distance, repetitions=5))
    if any(r != record["results"][0] for r in record["results"]):
        raise ValueError("SDK/GEOS repeated offset geometry changed")
    values = record["results"][0]
    sdk = shapely.Polygon(values["sdk_vertices_xz"]) if values["sdk_vertices_xz"] else shapely.Polygon()
    own = shapely.union_all([shapely.Polygon(ring) for ring in values["own_rings_xy"]])
    if not sdk.is_valid or not own.is_valid:
        raise ValueError("offset emitted invalid geometry")
    error = sdk.symmetric_difference(own).area
    record.update(id=name, distance_m=float(np.float32(distance)), input_wkt=quantized.wkt, sdk_wkt=sdk.wkt, python_wkt=own.wkt,
                  input_holes=0, sdk_components=int(not sdk.is_empty), python_components=len(shapely.get_parts(own)),
                  symmetric_difference_m2=error, hausdorff_m=float(sdk.hausdorff_distance(own)) if not sdk.is_empty and not own.is_empty else None,
                  sdk_p50_operation_decode_ns=float(np.median(record["sdk_operation_and_decode_ns"])),
                  python_p50_operation_decode_ns=float(np.median(record["python_operation_and_decode_ns"])))
    return record


def own_triangles(polygon):
    rings = [np.asarray(polygon.exterior.coords)[:-1], *[np.asarray(ring.coords)[:-1] for ring in polygon.interiors]]
    triangles = shapely.get_coordinates(shapely.get_parts(shapely.constrained_delaunay_triangles(polygon))).reshape(-1, 4, 2)[:, :3]
    xy, ids = np.unique(np.vstack((np.concatenate(rings), triangles.reshape(-1, 2))), axis=0, return_inverse=True)
    faces, _ = conform_triangles(xy, ids[sum(map(len, rings)):].reshape(-1, 3))
    p = xy[faces]
    u, v = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    reverse = u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0] < 0
    faces[reverse] = faces[reverse][:, [0, 2, 1]]
    return xy, faces


def triangulation_case(name, polygon):
    rings = [np.asarray(polygon.exterior.coords)[:-1].tolist(), *[np.asarray(ring.coords)[:-1].tolist() for ring in polygon.interiors]]
    record = reference(dict(operation="tessellation", rings=rings))
    vertices, raw_faces = np.asarray(record["vertices"]), np.asarray(record["triangles"])
    began = time.perf_counter_ns()
    p = vertices[raw_faces, :2]
    u, v = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    raw_area = u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]
    if np.any(raw_area < 0):
        raise ValueError("raw SDK tessellation reversed a nonzero face")
    faces, _ = conform_triangles(vertices[:, :2], raw_faces[raw_area != 0])
    sdk_adapter_ns = time.perf_counter_ns() - began
    began = time.perf_counter_ns()
    own_xy, own_faces = own_triangles(polygon)
    own_ns = time.perf_counter_ns() - began
    audits = {}
    for method, xy, indices in (("sdk_backend", vertices[:, :2], faces), ("python_cdt_conform", own_xy, own_faces)):
        p = xy[indices]
        u, v = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
        if np.any(u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0] <= 0):
            raise ValueError("triangulation has a zero or reversed face")
        triangles = shapely.polygons(xy[indices])
        if not shapely.coverage_is_valid(triangles):
            raise ValueError("triangulation is not an edge-matched coverage")
        actual = shapely.coverage_union_all(triangles)
        area = actual.symmetric_difference(polygon).area
        if area > max(1e-8, polygon.area * 1e-12):
            raise ValueError("triangulation lost its declared domain")
        edges, counts = np.unique(np.sort(np.vstack((indices[:, [0, 1]], indices[:, [1, 2]], indices[:, [2, 0]])), axis=1), axis=0, return_counts=True)
        if counts.max() > 2:
            raise ValueError("nonmanifold SDK/Python triangulation")
        boundary = shapely.linestrings(xy[edges[counts == 1]])
        if not np.all(shapely.covered_by(boundary, shapely.buffer(polygon.boundary, 1e-8))):
            raise ValueError("triangulation has an interior crack")
        used = np.unique(indices)
        declared = np.concatenate([np.asarray(r) for r in rings])
        missing = sum(not np.any(np.all(xy[used] == point, axis=1)) for point in declared)
        audits[method] = dict(vertices=len(xy), triangles=len(indices), symmetric_difference_m2=area,
                              boundary_edges=int((counts == 1).sum()), unused_declared_boundary_vertices=missing)
    np.savez_compressed(OUTPUT / (name + "-triangles.npz"), sdk_vertices=vertices, sdk_faces=faces, sdk_raw_faces=raw_faces,
                        own_vertices=np.c_[own_xy, np.zeros(len(own_xy))], own_faces=own_faces)
    record.update(id=name, input_wkt=polygon.wkt, audit=audits, python_cdt_conform_ns=own_ns,
                  raw_sdk_zero_faces=int((raw_area == 0).sum()), sdk_mesh_adapter_ns=sdk_adapter_ns,
                  mesh_adapter="Raw SDK raster triangles retained separately; exact-zero faces excluded and declared boundary vertices conformed by the shared Python operator")
    return record


def real_intersections(limit=256):
    sources = json.loads(INPUT.read_text())
    lines, owners, vectors = [], [], []
    for road in sources["roads"]:
        xy = np.asarray(road["coordinates"], dtype=float)
        for a, b in zip(xy[:-1], xy[1:]):
            if np.linalg.norm(a - b) > 1e-8:
                vectors.append(np.c_[np.array([a, b]), np.zeros(2)])
                lines.append(shapely.LineString([a, b]))
                owners.append(dict(source_id=road["source_id"], properties=road["properties"]))
            if len(lines) == 1024:
                break
        if len(lines) == 1024:
            break
    tree, cases, identities = shapely.STRtree(lines), [], []
    for i, line in enumerate(lines):
        for j in sorted(tree.query(line, predicate="intersects")):
            if i >= j or owners[i]["source_id"] == owners[j]["source_id"]:
                continue
            cases.append(np.vstack((vectors[i], vectors[j])))
            identities.append([owners[i], owners[j]])
            if len(cases) == limit:
                break
        if len(cases) == limit:
            break
    record = reference(dict(operation="edges", vectors=np.asarray(cases).tolist()))
    categories, accepted_errors = [], []
    for vectors, result in zip(cases, record["results"]):
        a, b = shapely.LineString(vectors[:2, :2]), shapely.LineString(vectors[2:, :2])
        intersection = a.intersection(b)
        if intersection.geom_type != "Point":
            category = "collinear_overlap"
        else:
            point = np.asarray(intersection.coords)[0]
            category = "endpoint" if np.linalg.norm(vectors[:, :2] - point, axis=1).min() < 1e-8 else "proper_crossing"
        categories.append(category)
        if result["accepted"]:
            p = shapely.Point(result["candidate_xyz"][:2])
            accepted_errors.append(max(p.distance(a), p.distance(b)))
    if accepted_errors and max(accepted_errors) > 1e-7:
        raise ValueError("accepted planar SDK contact left the source segments")
    record.update(case_sources=identities, vectors=np.asarray(cases).tolist(), closed_planar_intersections=len(cases),
                  sdk_accepted=sum(r["accepted"] for r in record["results"]),
                  closed_planar_sdk_disagreements=sum(not r["accepted"] for r in record["results"]),
                  case_categories=categories, maximum_accepted_source_distance_m=max(accepted_errors, default=0.),
                  scope="Real planar-source segment pairs; GEOS closed endpoints versus observed SDK half-open/closest-mode semantics, not 3D contact equivalence")
    return record


def offset_costs(cases, offsets):
    selected, reports = [], []
    for case, proof in zip(cases, offsets):
        if (proof.get("sdk_components") == proof.get("python_components") == 1
                and proof.get("hausdorff_m", 1.) is not None and proof.get("hausdorff_m", 1.) <= 1e-4):
            selected.append(case)
        if len(selected) == 4:
            break
    for name, polygon, distance in selected:
        for backend in ("sdk", "python"):
            request = dict(operation="offset", backend=backend, xy=np.asarray(polygon.exterior.coords, dtype=np.float32)[:-1].tolist(),
                           distance=distance, repetitions=5)
            workers = [reference(request) for _ in range(3)]
            field = "sdk_operation_and_decode_ns" if backend == "sdk" else "python_operation_and_decode_ns"
            if len({json.dumps(w["results"]) for w in workers}) != 1:
                raise ValueError("isolated cost workers changed offset geometry")
            reports.append(dict(id=name, backend=backend, p50_prepared_operation_decode_ns=float(np.median([v for w in workers for v in w[field]])),
                                peak_rss_bytes=max(w["peak_rss_bytes"] for w in workers),
                                whole_child_ms=[w["whole_child_ms"] for w in workers], samples=[w[field] for w in workers],
                                sdk_loaded=workers[0]["sdk_dependency_loaded"],
                                quality="same float32 input; one component and full output boundary within1e-4m across methods",
                                timing_scope="Prepared polygon operation and owned XY output; SDK also validates its private output memory ranges. Process startup/source preparation excluded."))
    return reports


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    real, skipped = real_parts()
    cases = controls() + real
    offsets = [offset_case(*case) for case in cases]
    triangles = [triangulation_case(name, polygon) for name, polygon, _ in cases[:5] + real[:8]]
    contacts = real_intersections()
    costs = offset_costs(cases, offsets)
    sources = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in (INPUT, SURFACES)}
    report = dict(commit=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                  source_files=sources, source_receipt=json.loads(INPUT.read_text())["receipt"],
                  offsets=offsets, triangulations=triangles, intersections=contacts, isolated_offset_costs=costs, skipped_reference_parts=skipped,
                  limits="External Linux children: CPU8s, address space2GiB, parent wait10s. Tessellation backend differs from unverified util::Triangulator wrapper.",
                  remaining="Processor extrusion/mesh-plane/main-axis object ABIs, all SDK corner/material modes and complete OSM generator remain unverified; no SDK/runtime/library dependency introduced")
    (OUTPUT / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(dict(offset_cases=len(offsets), triangulation_cases=len(triangles), contact_cases=len(contacts["results"]),
                          offset_unavailable=sum("unavailable_reason" in r for r in offsets), closed_planar_semantic_differences=contacts["closed_planar_sdk_disagreements"])))


if __name__ == "__main__":
    main()
