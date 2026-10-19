"""Attribute-preserving arrays from the existing native capture input contract."""

import numpy as np
from packed_normals import unpack
from triangle_image_reference import intersect_source

REPRESENTATIONS = ("Fine", "Full", "Captured")
BINDING_COLUMNS = ("slot", "source_index", "part", "face", "stream", "triangle")
TRIANGLE_LIMIT = 131072


def chunks(source, representation):
    if representation == "Fine":
        return [source["native_fine_reference"]]
    return source[{"Full": "reference", "Captured": "captured"}[representation]]


def mesh(call, representation):
    """Retain original chunk/stream triangle order and unknown identities as -1."""
    sources = call["capture"]["sources"]
    count = sum(
        len(chunk[name]["triangles"])
        for source in sources
        for chunk in chunks(source, representation)
        for name in ("walls", "roofs")
    )
    if not 0 < count <= TRIANGLE_LIMIT:
        raise ValueError("nonempty mesh within diagnostic triangle budget required")
    arrays = {key: [] for key in ("positions", "uv", "normal", "colour", "metres")}
    indices, bindings, offset = [], [], 0
    for source in sources:
        slot = source["slot"]
        original = call["members"][slot]["source"]
        for chunk in chunks(source, representation):
            for stream, name in enumerate(("walls", "roofs")):
                data = chunk[name]
                n = len(data["positions"])
                if not data["triangles"]:
                    continue
                arrays["positions"].extend(data["positions"])
                arrays["uv"].extend(data["uv"])
                arrays["normal"].extend(data["normal_words"])
                colour = chunk["wall_colours"] if stream == 0 else []
                metres = chunk["wall_material_metres"] if stream == 0 else []
                arrays["colour"].extend(
                    np.reshape(colour, (n, 4)) if colour else [[1] * 4] * n
                )
                arrays["metres"].extend(
                    np.reshape(metres, (n, 2)) if metres else [[0] * 2] * n
                )
                indices.extend(np.asarray(data["triangles"], np.int64) + offset)
                prefix = [slot, original, chunk["part"], chunk["face_index"], stream]
                bindings.extend(
                    [*(-1 if value is None else value for value in prefix), triangle]
                    for triangle in range(len(data["triangles"]))
                )
                offset += n
    result = {
        key: np.asarray(values, np.uint32 if key == "normal" else np.float32)
        for key, values in arrays.items()
    }
    result["indices"] = np.asarray(indices, np.int64)
    result["binding"] = np.asarray(bindings, np.int32)
    result["vertex_normal"] = unpack(result["normal"])
    return result


def sample(source, prepared, origins, directions):
    """Shared front-face ray reader; interpolate native fields at the actual hit."""
    if len(origins) > 32:
        parts = [
            sample(
                source,
                prepared,
                origins[begin : begin + 32],
                directions[begin : begin + 32],
            )
            for begin in range(0, len(origins), 32)
        ]
        return (
            np.concatenate([part[0] for part in parts]),
            np.concatenate([part[1] for part in parts]),
            {
                name: np.concatenate([part[2][name] for part in parts])
                for name in parts[0][2]
            },
        )
    depth, faces = intersect_source(source, origins, directions, prepared=prepared)
    return depth, faces, fields_at(source, origins, directions, depth, faces)


def fields_at(source, origins, directions, depth, faces):
    """Interpolate unchanged source inputs at already identified original hits."""
    valid = faces >= 0
    fields = {"normal": np.zeros((len(faces), 3))}
    for name, width in (("colour", 4), ("uv", 2), ("metres", 2)):
        fields[name] = np.zeros((len(faces), width))
    triangle = source["indices"][faces[valid]]
    xyz = source["positions"][triangle].astype(float)
    edge1, edge2 = xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0]
    hit = origins[valid] + depth[valid, None] * directions[valid] - xyz[:, 0]
    normal = np.cross(edge1, edge2)
    determinant = np.einsum("ij,ij->i", normal, normal)
    u = np.einsum("ij,ij->i", np.cross(hit, edge2), normal) / determinant
    v = np.einsum("ij,ij->i", np.cross(edge1, hit), normal) / determinant
    weights = np.column_stack((1 - u - v, u, v))
    for name, values in fields.items():
        attribute = source["vertex_normal" if name == "normal" else name][triangle]
        values[valid] = np.einsum("ij,ijk->ik", weights, attribute)
    length = np.linalg.norm(fields["normal"][valid], axis=1)
    if np.any(length == 0):
        raise ValueError("undefined interpolated normal")
    fields["normal"][valid] /= length[:, None]
    fields["colour"] = fields["colour"][:, :3]
    return fields


def independent_depth(triangles, origins, directions):
    """Moller-Trumbore all-triangle control, independent of R-tree hit selection."""
    a, b, c = np.asarray(triangles, float).transpose(1, 0, 2)
    edge1, edge2 = b - a, c - a
    answer = np.full(len(origins), np.inf)
    for begin in range(0, len(origins), 16):
        o = origins[begin : begin + 16]
        d = directions[begin : begin + 16]
        p = np.cross(d[:, None], edge2)
        determinant = np.einsum("tj,rtj->rt", edge1, p)
        front = determinant > 1e-12
        reciprocal = np.divide(
            1.0, determinant, out=np.zeros_like(determinant), where=front
        )
        relative = o[:, None] - a
        u = np.einsum("rtj,rtj->rt", relative, p) * reciprocal
        q = np.cross(relative, edge1)
        v = np.einsum("rj,rtj->rt", d, q) * reciprocal
        t = np.einsum("tj,rtj->rt", edge2, q) * reciprocal
        keep = front & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 0.1)
        answer[begin : begin + len(o)] = np.where(keep, t, np.inf).min(axis=1)
    return answer
