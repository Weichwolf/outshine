"""One shared original program pool behind an adaptive directional index."""

import json
from pathlib import Path

import numpy as np
from attributed_prisms import native_fields, native_inputs
from beam_field_plan import build
from packed_normals import unpack
from trimesh.constants import tol
from trimesh.intersections import planes_lines
from trimesh.triangles import normals, points_to_barycentric
from viewcell_product import compact, require_domain


def cook(
    source,
    observer_box,
    profile,
    path,
    provenance,
    *,
    fuse=True,
    certified_opaque=False,
):
    arrays, work = build(
        source, observer_box, profile, fuse=fuse, certified_opaque=certified_opaque
    )
    faces = np.unique(arrays["references"])
    pool = compact(source, faces)
    mapping = np.full(len(source["indices"]), -1, np.int32)
    mapping[faces] = np.arange(len(faces), dtype=np.int32)
    arrays["references"] = mapping[arrays["references"]].astype(np.uint32)
    path = Path(path)
    path.mkdir(parents=True, exist_ok=False)
    np.savez_compressed(path / "index.npz", **arrays)
    np.savez_compressed(path / "programs.npz", **pool)
    header = {
        "format": "adaptive-opaque-beam-field-1",
        "observer_bounds": np.asarray(observer_box).tolist(),
        "provenance": provenance,
        "profile": profile,
        "index_arrays": {k: a.nbytes for k, a in arrays.items()},
        "program_arrays": {k: a.nbytes for k, a in pool.items()},
        "stored_array_bytes": sum(a.nbytes for a in (*arrays.values(), *pool.values())),
        "work": work,
        "scope": "continuous interval selection, original candidate geometry/attributes; primary opaque rays only, no PBR/caster/world/device admission",
    }
    (path / "header.json").write_text(json.dumps(header, indent=2) + "\n")
    return header


def load_product(path, observers, provenance):
    path = Path(path)
    header = json.loads((path / "header.json").read_text())
    if (
        header["format"] != "adaptive-opaque-beam-field-1"
        or header["provenance"] != provenance
    ):
        raise ValueError("directional field identity or format mismatch")
    require_domain(header["observer_bounds"], observers)
    arrays = {}
    for name, target in (("index", arrays), ("programs", arrays)):
        with np.load(path / (name + ".npz"), allow_pickle=False) as archive:
            target.update({k: archive[k] for k in archive.files})
    arrays["vertex_normal"] = unpack(arrays["normal"])
    if not len(arrays["colour"]):
        arrays["colour"] = np.broadcast_to(
            np.ones(4, np.float32), (len(arrays["positions"]), 4)
        )
    if not len(arrays["metres"]):
        arrays["metres"] = np.broadcast_to(
            np.zeros(2, np.float32), (len(arrays["positions"]), 2)
        )
    triangle = arrays["positions"][arrays["indices"]].astype(float)
    geometric = np.cross(
        triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0]
    )
    unit, _ = normals(triangle)
    derived = (
        triangle.nbytes
        + geometric.nbytes
        + unit.nbytes
        + arrays["vertex_normal"].nbytes
    )
    inputs = native_inputs(arrays)
    stage_bytes = inputs["uv"].nbytes + inputs["flat_codes"].nbytes
    return {
        "header": header,
        "arrays": arrays,
        "source": arrays,
        "inputs": inputs,
        "triangle": triangle,
        "geometric": geometric,
        "unit": unit,
        "derived_cpu_bytes": derived + stage_bytes,
        "vertex_stage_copy_bytes": stage_bytes,
        "stored_array_bytes": header["stored_array_bytes"],
        "decoded_normal_bytes": arrays["vertex_normal"].nbytes,
        "work": {
            "rays": 0,
            "node_ray_tests": 0,
            "candidate_plane_tests": 0,
            "largest_candidate_pairs": 0,
        },
    }


def route(arrays, origins, directions):
    basis = arrays["basis"]
    d = directions @ basis.T
    q = np.divide(
        d[:, :2], d[:, 2, None], out=np.zeros((len(d), 2)), where=d[:, 2, None] > 0
    )
    box = arrays["bounds"]
    active = (d[:, 2] > 0) & np.all((q >= box[0]) & (q <= box[1]), axis=1)
    rays = np.flatnonzero(active)
    nodes = np.zeros(len(rays), np.int32)
    low, high = (
        np.broadcast_to(box[0], (len(rays), 2)).copy(),
        np.broadcast_to(box[1], (len(rays), 2)).copy(),
    )
    work = 0
    for _ in range(21):
        internal = arrays["children"][nodes, 0] >= 0
        work += len(nodes)
        if not internal.any():
            return rays, nodes, work
        at = np.flatnonzero(internal)
        middle = (low[at] + high[at]) / 2
        upper = q[rays[at]] >= middle
        child = upper[:, 0].astype(int) + 2 * upper[:, 1]
        nodes[at] = arrays["children"][nodes[at], child]
        low[at], high[at] = (
            np.where(upper, middle, low[at]),
            np.where(upper, high[at], middle),
        )
    raise RuntimeError("directional field exceeds declared finite depth")


def intersections(product, origins, directions):
    a = product["arrays"]
    rays, nodes, node_work = route(a, origins, directions)
    result, selected = (
        np.full(len(origins), np.inf),
        np.full(len(origins), -1, np.int64),
    )
    work = product["work"]
    work["rays"] += len(origins)
    work["node_ray_tests"] += node_work
    for begin in range(0, len(rays), 256):
        rr, nn = rays[begin : begin + 256], nodes[begin : begin + 256]
        offsets, counts = a["ranges"][nn].T
        pair_rays = np.repeat(rr, counts)
        starts = np.repeat(np.cumsum(counts, dtype=np.int64) - counts, counts)
        slots = np.repeat(offsets, counts) + np.arange(len(pair_rays)) - starts
        faces = a["references"][slots.astype(np.int64)]
        work["candidate_plane_tests"] += len(faces)
        work["largest_candidate_pairs"] = max(
            work["largest_candidate_pairs"], len(faces)
        )
        if not len(faces):
            continue
        triangles = product["triangle"][faces]
        points, valid = planes_lines(
            triangles[:, 0],
            product["unit"][faces],
            origins[pair_rays],
            directions[pair_rays],
        )
        bary = points_to_barycentric(triangles[valid], points)
        hit = (bary > -tol.zero).all(1) & (bary < 1 + tol.zero).all(1)
        rr, ff = pair_rays[valid][hit], faces[valid][hit]
        n = product["geometric"][ff]
        den = np.einsum("ij,ij->i", n, directions[rr])
        t = np.divide(
            np.einsum("ij,ij->i", triangles[valid][hit, 0] - origins[rr], n),
            den,
            out=np.full(len(rr), np.inf),
            where=den != 0,
        )
        keep = (den < -1e-12) & (t > 0.1)
        rr, ff, t = rr[keep], ff[keep], t[keep]
        order = np.lexsort((a["original_face"][ff], t, rr))
        rr, ff, t = rr[order], ff[order], t[order]
        first = np.r_[True, rr[1:] != rr[:-1]] if len(rr) else np.empty(0, bool)
        result[rr[first]], selected[rr[first]] = t[first], ff[first]
    return result, selected


def query(product, origins, directions):
    origins = np.asarray(origins, float)
    require_domain(product["header"]["observer_bounds"], origins)
    directions = np.asarray(directions, float)
    profile = product["header"]["profile"]
    maximum = np.sqrt(
        1
        + (profile["width"] / (2 * profile["focal_px"])) ** 2
        + (profile["height"] / (2 * profile["focal_px"])) ** 2
    )
    if (
        directions.shape != np.shape(origins)
        or not np.isfinite(directions).all()
        or np.any(np.linalg.norm(directions, axis=1) > maximum * (1 + 1e-12))
        or np.any(np.linalg.norm(directions, axis=1) == 0)
    ):
        raise ValueError("direction outside finite near-clip profile")
    depth, faces = intersections(product, np.asarray(origins), directions)
    fields = native_fields(product["inputs"], origins, directions, depth, faces)
    owner = np.full(len(faces), -1, np.int32)
    tags = owner.copy()
    covered = faces >= 0
    owner[covered] = product["arrays"]["binding"][faces[covered], 1]
    tags[covered] = product["arrays"]["original_face"][faces[covered]]
    return depth, fields, owner, tags
