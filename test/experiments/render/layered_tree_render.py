"""Load selected field blocks, reconstruct finite patches, query without source geometry."""

from pathlib import Path

import numpy as np
from layered_tree_plan import Plan
from packed_normals import unpack
from triangle_image_reference import intersect_source, prepare_source


def load_header(directory):
    with np.load(Path(directory) / "header.npz", allow_pickle=False) as f:
        header = {k: f[k] for k in f.files}
    return Plan(header["bounds"], header["children"], int(header["resolution"])), header


def clip(polygon, axis, bound, sign):
    if not len(polygon):
        return polygon
    answer = []
    for a, b in zip(polygon, np.roll(polygon, -1, axis=0), strict=True):
        da, db = sign * (a[axis] - bound), sign * (b[axis] - bound)
        if da <= 0:
            answer.append(a)
        if (da < 0 < db) or (db < 0 < da):
            answer.append(a + (b - a) * (da / (da - db)))
    return np.array(answer).reshape(-1, 3)


def patches(box, resolution, data):
    normal = unpack(data["geometric_normal"])
    triangles = []
    owners = []
    centres = []
    groups = []
    for group in range(6):
        begin, end = data["offsets"][group : group + 2]
        axis = group // 2
        other = [i for i in range(3) if i != axis]
        step = np.ptp(box, axis=0) / resolution
        for record in range(begin, end):
            pixel = int(data["pixel"][record])
            p = box[0].copy()
            p[other] = (
                box[0, other]
                + (np.array([pixel % resolution, pixel // resolution]) + 0.5)
                * step[other]
            )
            p[axis] += float(data["depth"][record])
            centres.append(p)
            groups.append(group)
            n = normal[record]
            if abs(n[axis]) < 0.5:
                raise ValueError("dominant-axis footprint required")
            q = np.broadcast_to(p, (4, 3)).copy()
            q[:, other] += (
                np.array([[-0.5, -0.5], [0.5, -0.5], [0.5, 0.5], [-0.5, 0.5]])
                * step[other]
            )
            q[:, axis] -= ((q[:, other] - p[other]) @ n[other]) / n[axis]
            if np.dot(np.cross(q[1] - q[0], q[2] - q[0]), n) < 0:
                q = q[::-1]
            for k in range(3):
                q = clip(q, k, box[0, k], -1)
                q = clip(q, k, box[1, k], 1)
            for k in range(1, len(q) - 1):
                tri = q[[0, k, k + 1]]
                if np.linalg.norm(np.cross(tri[1] - tri[0], tri[2] - tri[0])) > 1e-12:
                    triangles.append(tri)
                    owners.append(record)
    return (
        np.array(triangles).reshape(-1, 3, 3),
        np.array(owners, np.int32),
        np.array(centres).reshape(-1, 3),
        np.array(groups, np.uint8),
    )


def dense(data, key, channels):
    result = np.zeros((len(data["depth"]), 3, channels), np.float32)
    if key == "colour":
        result[:, 0] = 1
    index = data[key + "_index"] if key == "colour" else data["metre_index"]
    if len(index):
        have = index >= 0
        result[have] = data[key][index[have]]
    return result


def load_cut(directory, tree, nodes):
    if not len(nodes):
        raise ValueError("no resolved field nodes; original or finer products required")
    chunks = []
    triangles = []
    owners = []
    base = 0
    loaded = 0
    containers = 0
    for node in nodes:
        path = Path(directory) / "nodes" / f"{node:06d}.npz"
        with np.load(path, allow_pickle=False) as f:
            data = {k: f[k] for k in f.files}
        loaded += sum(a.nbytes for a in data.values())
        containers += path.stat().st_size
        tri, record, centres, groups = patches(tree.bounds[node], tree.resolution, data)
        attrs = {
            k: data[k]
            for k in ("normal", "code", "coordinates", "source", "original_face")
        }
        attrs.update(
            metres=dense(data, "metres", 2),
            colour=dense(data, "colour", 4) if len(data["colour_index"]) else None,
            centres=centres,
            groups=groups,
        )
        chunks.append(attrs)
        triangles.append(tri)
        owners.append(record + base)
        base += len(data["depth"])
    values = {
        k: np.concatenate([c[k] for c in chunks]) for k in chunks[0] if k != "colour"
    }
    if any(c["colour"] is not None for c in chunks):
        colours = []
        for c in chunks:
            value = c["colour"]
            if value is None:
                value = np.zeros((len(c["source"]), 3, 4), np.float32)
                value[:, 0] = 1
            colours.append(value)
        values["colour"] = np.concatenate(colours)
    else:
        values["colour"] = np.empty((0, 3, 4), np.float32)
    xyz = np.concatenate(triangles)
    record = np.concatenate(owners)
    order = np.lexsort((record, values["original_face"][record]))
    xyz = xyz[order]
    record = record[order]
    source = {
        "positions": xyz.reshape(-1, 3),
        "indices": np.arange(xyz.size // 3, dtype=np.uint32).reshape(-1, 3),
    }
    prepared = prepare_source(source) if len(source["indices"]) else None
    return {
        "source": source,
        "prepared": prepared,
        "records": values,
        "triangle_record": record,
        "cost": {
            "loaded_field_bytes": loaded,
            "loaded_container_bytes": containers,
            "derived_attribute_bytes": sum(a.nbytes for a in values.values()),
            "proxy_geometry_bytes": sum(a.nbytes for a in source.values())
            + record.nbytes,
            "proxy_triangles": len(record),
            "patch_records": base,
            "loaded_nodes": len(nodes),
        },
    }


def query(cut, origins, directions):
    n = len(origins)
    if cut["prepared"] is None:
        depth = np.full(n, np.inf)
        faces = np.full(n, -1, np.int64)
    else:
        depth, faces = intersect_source(
            cut["source"], origins, directions, prepared=cut["prepared"]
        )
    hit = faces >= 0
    record = cut["triangle_record"][faces[hit]]
    data = cut["records"]
    point = origins[hit] + depth[hit, None] * directions[hit]
    group = data["groups"][record] // 2
    other = np.array([[1, 2], [0, 2], [0, 1]])[group]
    delta = np.take_along_axis(point - data["centres"][record], other, axis=1)
    fields = {
        "normal": np.zeros((n, 3)),
        "code": np.zeros(n, np.int32),
        "coordinates": np.zeros((n, 2)),
        "metres": np.zeros((n, 2)),
        "colour": np.zeros((n, 4)),
    }
    fields["normal"][hit] = unpack(data["normal"][record])
    fields["code"][hit] = data["code"][record]
    for key in ("coordinates", "metres", "colour"):
        if key == "colour" and not len(data[key]):
            fields[key][hit] = 1
            continue
        a = data[key][record]
        fields[key][hit] = a[:, 0] + np.einsum("ni,nij->nj", delta, a[:, 1:])
    source = np.full(n, -1, np.int32)
    original = np.full(n, -1, np.int64)
    source[hit] = data["source"][record]
    original[hit] = data["original_face"][record]
    return depth, fields, source, original
