"""CPU query of a ready analytic projected field; no original triangle buffers."""

import itertools
import json
from pathlib import Path

import numpy as np
from packed_normals import unpack
from projective_field_bins import pairs
from viewcell_product import require_domain


def prepare(arrays, header):
    footprint = arrays["footprint"].astype(float)
    corners_q = arrays["inverse_depth"].astype(float)
    edges = footprint[:, 1:] - footprint[:, :1]
    q = np.column_stack(
        (
            corners_q[:, 0],
            np.linalg.solve(edges, (corners_q[:, 1:] - corners_q[:, :1])[..., None])[
                ..., 0
            ],
        )
    )
    first = footprint[:, 0]
    focal = float(arrays["focal_px"])
    planes = np.column_stack(
        (q[:, 1] * focal, q[:, 2] * focal, q[:, 0] - np.sum(q[:, 1:] * first, axis=1))
    )
    low = arrays["bin_low"]
    high = low + arrays["bin_shape"] * float(arrays["bin_pitch"])
    zlo, zhi = arrays["depth_range"]
    local_bounds = np.array(
        [
            [u / focal * z, v / focal * z, z]
            for u, v, z in itertools.product(
                (low[0], high[0]), (low[1], high[1]), (zlo, zhi)
            )
        ]
    )
    extent = arrays["eye"] + local_bounds @ arrays["basis"]
    normal = unpack(arrays["normal"])
    return {
        "arrays": arrays,
        "header": header,
        "planes": planes,
        "normal": normal,
        "source": {"positions": extent},
        "derived_cpu_bytes": planes.nbytes + normal.nbytes + extent.nbytes,
        "work": {
            "rays": 0,
            "bin_visits": 0,
            "program_references_before_dedup": 0,
            "candidate_plane_tests": 0,
            "trim_tests": 0,
            "largest_candidate_pairs": 0,
            "field_evaluations": 0,
        },
    }


def load_product(path, observers, provenance):
    path = Path(path)
    header = json.loads((path / "header.json").read_text())
    if (
        header["format"] != "outshine-projective-vector-field-v1"
        or header["provenance"] != provenance
    ):
        raise ValueError("projected field identity/format mismatch")
    require_domain(header["coordinate_bounds"], observers)
    with np.load(path / "field.npz", allow_pickle=False) as archive:
        arrays = {key: archive[key] for key in archive.files}
    return prepare(arrays, header)


def intersections(product, origins, directions):
    a = product["arrays"]
    origin = (origins - a["eye"]) @ a["basis"].T
    direction = directions @ a["basis"].T
    low = a["bin_low"]
    high = low + a["bin_shape"] * float(a["bin_pitch"])
    focal = float(a["focal_px"])
    zlo, zhi = a["depth_range"]
    normals = np.array(
        [
            [focal, 0, -low[0]],
            [-focal, 0, high[0]],
            [0, focal, -low[1]],
            [0, -focal, high[1]],
            [0, 0, 1],
            [0, 0, -1],
        ]
    )
    offsets = np.array([0, 0, 0, 0, -zlo, zhi])
    value = origin @ normals.T + offsets
    slope = direction @ normals.T
    boundary = np.divide(-value, slope, out=np.zeros_like(value), where=slope != 0)
    near = np.maximum(
        np.where(slope > 0, boundary, -np.inf).max(1), np.nextafter(0.1, np.inf)
    )
    far = np.where(slope < 0, boundary, np.inf).min(1)
    active = (far >= near) & np.isfinite(far) & ~((slope == 0) & (value < 0)).any(1)
    first3 = origin + near[:, None] * direction
    last3 = origin + far[:, None] * direction
    # Inactive rays receive harmless finite coordinates and are excluded by pairs.
    first = float(a["focal_px"]) * np.divide(
        first3[:, :2],
        first3[:, 2, None],
        out=np.zeros((len(origin), 2)),
        where=active[:, None],
    )
    last = float(a["focal_px"]) * np.divide(
        last3[:, :2],
        last3[:, 2, None],
        out=np.zeros((len(origin), 2)),
        where=active[:, None],
    )
    rays, faces, work = pairs(a, first, last, active)
    work["candidate_plane_tests"] = len(faces)
    work["largest_candidate_pairs"] = len(faces)
    result, selected = np.full(len(origin), np.inf), np.full(len(origin), -1, np.int64)
    if not len(faces):
        work["trim_tests"] = 0
        return result, selected, work
    plane = product["planes"][faces]
    den = np.einsum("ij,ij->i", plane, direction[rays])
    numerator = 1 - np.einsum("ij,ij->i", plane, origin[rays])
    t = np.divide(numerator, den, out=np.full(len(faces), np.inf), where=den != 0)
    footprint = a["footprint"][faces].astype(float)
    e, f = footprint[:, 1] - footprint[:, 0], footprint[:, 2] - footprint[:, 0]
    determinant = e[:, 0] * f[:, 1] - e[:, 1] * f[:, 0]
    valid = np.isfinite(t) & (t > 0.1) & (den * determinant < 0)
    work["trim_tests"] = int(valid.sum())
    rays, faces, t, footprint, e, f, determinant = (
        v[valid] for v in (rays, faces, t, footprint, e, f, determinant)
    )
    point = origin[rays] + t[:, None] * direction[rays]
    uv = float(a["focal_px"]) * point[:, :2] / point[:, 2, None]
    delta = uv - footprint[:, 0]
    u = (delta[:, 0] * f[:, 1] - delta[:, 1] * f[:, 0]) / determinant
    v = (e[:, 0] * delta[:, 1] - e[:, 1] * delta[:, 0]) / determinant
    valid = (u >= 0) & (v >= 0) & (u + v <= 1)
    rays, faces, t = (value[valid] for value in (rays, faces, t))
    order = np.lexsort((a["original_face"][faces], t, rays))
    rays, faces, t = (value[order] for value in (rays, faces, t))
    first_hit = np.r_[True, rays[1:] != rays[:-1]] if len(rays) else np.empty(0, bool)
    result[rays[first_hit]], selected[rays[first_hit]] = t[first_hit], faces[first_hit]
    return result, selected, work


def evaluate(table, faces, weights):
    return np.einsum("ni,nik->nk", weights, table[faces])


def attributes(product, origins, directions, depth, faces):
    a = product["arrays"]
    hit = faces >= 0
    ids = faces[hit]
    local = (origins[hit] + depth[hit, None] * directions[hit] - a["eye"]) @ a[
        "basis"
    ].T
    xy = float(a["focal_px"]) * local[:, :2] / local[:, 2, None]
    delta = xy - a["footprint"][ids, 0]
    footprint = a["footprint"][ids].astype(float)
    e, f = footprint[:, 1] - footprint[:, 0], footprint[:, 2] - footprint[:, 0]
    determinant = e[:, 0] * f[:, 1] - e[:, 1] * f[:, 0]
    u = (delta[:, 0] * f[:, 1] - delta[:, 1] * f[:, 0]) / determinant
    v = (e[:, 0] * delta[:, 1] - e[:, 1] * delta[:, 0]) / determinant
    weights = np.column_stack((1 - u - v, u, v)) * a["inverse_depth"][ids]
    weights /= weights.sum(1, keepdims=True)
    fields = {
        "normal": np.zeros((len(faces), 3)),
        "code": np.zeros(len(faces), np.int32),
        "coordinates": np.zeros((len(faces), 2)),
        "metres": np.zeros((len(faces), 2)),
        "colour": np.zeros((len(faces), 4)),
    }
    fields["normal"][hit] = product["normal"][ids]
    fields["code"][hit] = a["code"][ids]
    fields["coordinates"][hit] = evaluate(a["coordinates"], ids, weights)
    fields["colour"][hit] = 1
    for key, index_key, target in (
        ("metres", "metre_index", "metres"),
        ("colour_delta", "colour_index", "colour"),
    ):
        if not len(a[index_key]):
            continue
        index = a[index_key][ids]
        present = index != np.iinfo(index.dtype).max
        values = evaluate(a[key], index[present], weights[present])
        fields[target][np.flatnonzero(hit)[present]] += values
    owner, original_face = (
        np.full(len(faces), -1, np.int32),
        np.full(len(faces), -1, np.int32),
    )
    owner[hit] = a["lineage"][a["lineage_index"][ids], 1]
    original_face[hit] = a["original_face"][ids]
    return fields, owner, original_face


def query(product, origins, directions):
    origins, directions = np.asarray(origins, float), np.asarray(directions, float)
    require_domain(product["header"]["coordinate_bounds"], origins)
    if (
        directions.shape != origins.shape
        or not np.isfinite(directions).all()
        or np.any(np.linalg.norm(directions, axis=1) == 0)
    ):
        raise ValueError("finite nonzero paired query directions required")
    depths, selected = [], []
    for begin in range(0, len(origins), 128):
        depth, faces, work = intersections(
            product, origins[begin : begin + 128], directions[begin : begin + 128]
        )
        depths.append(depth)
        selected.append(faces)
        for key, value in work.items():
            product["work"][key] = (
                max(product["work"][key], value)
                if key == "largest_candidate_pairs"
                else product["work"][key] + value
            )
    depth = np.concatenate(depths) if depths else np.empty(0)
    faces = np.concatenate(selected) if selected else np.empty(0, np.int64)
    fields, owner, tag = attributes(product, origins, directions, depth, faces)
    product["work"]["rays"] += len(origins)
    product["work"]["field_evaluations"] += int((faces >= 0).sum())
    return depth, fields, owner, tag
