"""Stored attributed procedural solids; categorical identity never travels through UV."""

from dataclasses import dataclass

import numpy as np
from native_capture_meshes import fields_at
from packed_normals import unpack
from prepared_material_fields import facade_catalog
from primitive_bvh import Hierarchy, build, trace
from roof_prisms import Prisms, compile_prisms
from triangle_image_reference import intersect_source, prepare_source


@dataclass
class Materials:
    frames: np.ndarray
    heights: np.ndarray
    edge: np.ndarray
    bays: np.ndarray
    codes: np.ndarray
    metres: np.ndarray


@dataclass
class IndexedMaterials(Materials):
    edge_offsets: np.ndarray
    code_index: np.ndarray | None = None


def material_tables(prisms, recipes):
    count = len(prisms.planes)
    frames, heights, metric = [], [], []
    edge = np.zeros((count, 4))
    bays = np.zeros(count, np.uint16)
    codes = np.zeros((count, 2), np.int32)
    catalog = facade_catalog()
    for primitive, (source, part) in enumerate(prisms.binding):
        surface = recipes[int(source)]
        shape = surface["shapes"][part]
        if surface["wall_colour"] is not None:
            raise ValueError("coloured surface must retain original fallback")
        frames.append([surface[k] for k in ("origin", "east", "north", "up")])
        base = shape["seat_m"] + shape["foot_m"]
        deck = (
            base
            + shape["eaves_m"]
            + (shape["rise_m"] if shape["roof_kind"] == 0 else 0)
        )
        heights.append([shape["floor_m"], shape["seat_m"], shape["foot_m"], deck])
        metric.append(
            shape["wall_variant"] == 6 and shape["opening_style"] != catalog["Glazing"]
        )
        begin, end = prisms.offsets[primitive : primitive + 2]
        codes[begin] = -(1 + 8 + 16 * shape["ident"])
        roof_code = -(1 + (2 if shape["roof_kind"] == 0 else 1) + 16 * shape["ident"])
        codes[begin + 1 : end] = roof_code
        for wall, a in enumerate(np.asarray(shape["ring"])):
            e = np.asarray(shape["ring"][(wall + 1) % len(shape["ring"])]) - a
            length = np.hypot(*e)
            plane = begin + 1 + wall
            edge[plane] = [*a, *e]
            bays[plane] = (
                0
                if length < 1.9 or shape["party_wall_edges"][wall]
                else np.clip(np.floor(length / shape["bay_m"] + 0.5), 1, 255)
            )
            back = shape["opening_style"] + len(catalog) * 3 * (
                shape["wall_variant"] % 8
            )
            codes[plane] = [
                back + (2 * len(catalog) if wall == shape["front_edge"] else 0),
                back,
            ]
    return Materials(
        np.array(frames).reshape(-1, 4, 3),
        np.array(heights).reshape(-1, 4),
        edge,
        bays,
        codes,
        np.array(metric, bool),
    )


def material_fields(materials, prisms, owners, planes, points):
    """Own analytic hit contract; Gable is evaluated at the hit, never each mesh corner."""
    if (
        np.any(owners < 0)
        or np.any(owners >= len(materials.frames))
        or len(owners) != len(points)
        or not np.isfinite(points).all()
    ):
        raise ValueError("finite represented hits required")
    if np.any(planes < prisms.offsets[owners]) or np.any(
        planes >= prisms.offsets[owners + 1]
    ):
        raise ValueError("material plane must belong to its primitive")
    frame = materials.frames[owners]
    local = np.einsum("ni,nji->nj", points - frame[:, 0], frame[:, 1:])
    floor, seat, foot, deck = materials.heights[owners].T
    gable = local[:, 2] > deck
    lookup = getattr(materials, "code_index", None)
    pair = materials.codes[planes if lookup is None else lookup[planes]]
    code = pair[np.arange(len(owners)), gable.astype(int)].astype(np.int32, copy=False)
    wall = code >= 0
    bay = np.remainder(code, 256).astype(np.float32)
    vertical = local[:, 2].astype(np.float32)
    metre = np.zeros((len(points), 2), np.float32)
    wall_indices = planes[wall]
    offsets = getattr(materials, "edge_offsets", None)
    if offsets is not None:
        wall_indices = (
            offsets[owners[wall]] + planes[wall] - prisms.offsets[owners[wall]] - 1
        )
    a, e = materials.edge[wall_indices, :2], materials.edge[wall_indices, 2:]
    length = np.hypot(e[:, 0], e[:, 1])
    dot = np.sum((local[wall, :2] - a) * e, axis=1)
    fraction = np.divide(dot, length * length, out=np.zeros(len(dot)), where=length > 0)
    bay[wall] = (
        np.where(gable[wall], 0, materials.bays[wall_indices]) * fraction
    ).astype(np.float32)
    vertical[wall] = np.float32(1) + (
        (local[wall, 2] - seat[wall] - foot[wall]) / floor[wall]
    ).astype(np.float32)
    metric = materials.metres[owners[wall]]
    take = np.flatnonzero(wall)[metric]
    metre[take, 0] = (dot[metric] / length[metric]).astype(np.float32)
    metre[take, 1] = (local[take, 2] - (seat[take] + foot[take])).astype(np.float32)
    return {
        "code": code,
        "coordinates": np.column_stack((bay, vertical)),
        "metres": metre,
        "normal": prisms.planes[planes, :3],
        "colour": np.ones((len(points), 4)),
    }


def compile_product(document, source, recipes):
    # Coloured sources retain their complete original attributes until the analytic
    # roof/colour-factor contract is supplied. Never silently whiten those sources.
    coloured = {i for i, r in recipes.items() if r and r["wall_colour"] is not None}
    geometry = document | {
        "sources": [
            item | {"surface": item["surface"] | {"projectable": False}}
            if item["index"] in coloured
            else item
            for item in document["sources"]
        ]
    }
    prisms, reasons = compile_prisms(geometry, source)
    reasons.update({i: "unsupported analytic colour-factor contract" for i in coloured})
    materials = material_tables(prisms, recipes)
    hierarchy = (
        build(prisms.bounds)
        if len(prisms.binding)
        else Hierarchy(np.empty((0, 2, 3)), np.empty((0, 2), np.int32))
    )
    original_faces = prisms.fallback_faces
    unique, inverse = np.unique(source["indices"][original_faces], return_inverse=True)
    fallback = {
        "positions": source["positions"][unique],
        "indices": inverse.reshape(-1, 3).astype(np.uint32),
        "uv": source["uv"][unique],
        "normal": source["normal"][unique],
        "metres": source["metres"][unique],
        "binding": source["binding"][original_faces],
    }
    colours = source["colour"][unique]
    fallback["colour"] = (
        colours if not np.all(colours == 1) else np.empty((0, 4), np.float32)
    )
    arrays = {
        **{"prism_" + k: v for k, v in vars(prisms).items()},
        **{"material_" + k: v for k, v in vars(materials).items()},
        **{"fallback_" + k: v for k, v in fallback.items()},
        "node_bounds": hierarchy.bounds,
        "node_children": hierarchy.children,
    }
    return arrays, reasons


def load_product(path):
    with np.load(path, allow_pickle=False) as data:
        arrays = {k: data[k] for k in data.files}

    def group(prefix):
        return {
            k.removeprefix(prefix): v for k, v in arrays.items() if k.startswith(prefix)
        }

    prisms = Prisms(**group("prism_"))
    material = group("material_")
    cls = IndexedMaterials if "edge_offsets" in material else Materials
    materials = cls(**material)
    hierarchy = Hierarchy(arrays["node_bounds"], arrays["node_children"])
    fallback = group("fallback_")
    fallback["vertex_normal"] = unpack(fallback["normal"])
    if not len(fallback["colour"]):
        fallback["colour"] = np.broadcast_to(
            np.ones(4, np.float32), (len(fallback["positions"]), 4)
        )
    prepared = (
        (prepare_source(fallback), native_inputs(fallback))
        if len(fallback["indices"])
        else None
    )
    return prisms, materials, hierarchy, fallback, prepared


def native_inputs(source):
    """Execute the actual vertex-stage category/mod rules once, before interpolation."""
    corners = source["uv"][source["indices"]]
    u = corners[..., 0]
    codes = np.where(u < 0, u, np.floor(u / 256)).astype(np.int32)
    if not (codes == codes[:, :1]).all():
        raise ValueError("original flat material code differs among triangle corners")
    uv = source["uv"].copy()
    uv[:, 0] = np.remainder(uv[:, 0], np.float32(256))
    return source | {"uv": uv, "flat_codes": codes[:, 0].copy()}


def native_fields(inputs, origins, directions, depth, faces):
    fields = fields_at(inputs, origins, directions, depth, faces, rgba=True)
    covered = faces >= 0
    fields["code"] = np.zeros(len(faces), np.int32)
    fields["code"][covered] = inputs["flat_codes"][faces[covered]]
    fields["coordinates"] = fields.pop("uv")
    return fields


def render_product(product, origins, directions):
    prisms, materials, hierarchy, fallback, prepared = product
    if len(prisms.binding):
        depth, owner, plane, work = trace(prisms, hierarchy, origins, directions)
    else:
        depth = np.full(len(origins), np.inf)
        owner = np.full(len(origins), -1, np.int64)
        plane = owner.copy()
        work = {"node_ray_tests": 0, "primitive_ray_tests": 0, "plane_ray_tests": 0}
    good = owner >= 0
    work["material_hit_queries"] = int(good.sum())

    work["fallback_field_queries"] = 0
    fields = {
        "code": np.zeros(len(depth), np.int32),
        "coordinates": np.zeros((len(depth), 2)),
        "metres": np.zeros((len(depth), 2)),
        "normal": np.zeros((len(depth), 3)),
        "colour": np.zeros((len(depth), 4)),
    }
    values = material_fields(
        materials,
        prisms,
        owner[good],
        plane[good],
        origins[good] + depth[good, None] * directions[good],
    )
    for k, field in fields.items():
        field[good] = values[k]
    wall_queries = int((values["code"] >= 0).sum())
    work["material_wall_queries"] = wall_queries
    offsets = getattr(materials, "edge_offsets", None)
    extra = (
        0 if offsets is None else offsets.dtype.itemsize + prisms.offsets.dtype.itemsize
    )
    lookup = getattr(materials, "code_index", None)
    code_bytes = 0 if lookup is None else lookup.dtype.itemsize
    base = (
        materials.frames.dtype.itemsize * 12
        + materials.heights.dtype.itemsize * 4
        + materials.codes.dtype.itemsize * 2
        + materials.metres.dtype.itemsize
        + prisms.planes.dtype.itemsize * 3
        + code_bytes
    )
    wall_bytes = (
        materials.edge.dtype.itemsize * 4 + materials.bays.dtype.itemsize + extra
    )
    work["material_record_operand_bytes"] = (
        base * int(good.sum()) + wall_bytes * wall_queries
    )
    source = np.full(len(depth), -1, np.int32)
    part = source.copy()
    source[good], part[good] = prisms.binding[owner[good]].T
    fallback_owner = np.full(len(depth), -1, np.int64)
    if prepared is not None:
        t, f = intersect_source(fallback, origins, directions, prepared=prepared[0])
        use = t < depth
        work["fallback_field_queries"] = int(np.isfinite(t).sum())
        values = native_fields(prepared[1], origins, directions, t, f)
        for k, field in fields.items():
            field[use] = values[k][use]
        depth[use] = t[use]
        if "binding" in fallback:
            source[use] = fallback["binding"][f[use], 1]
            part[use] = fallback["binding"][f[use], 2]
        else:
            source[use] = fallback["source_id"][f[use]]
            part[use] = fallback["part_id"][f[use]] if len(fallback["part_id"]) else -1
        fallback_owner[use] = prisms.fallback_faces[f[use]]
        owner[use], plane[use] = -1, -1
    work["attribute_output_bytes"] = sum(a.nbytes for a in fields.values())
    work["logical_box_plane_operand_bytes"] = (
        48 * work["node_ray_tests"] + 32 * work["plane_ray_tests"]
    )
    return depth, fields, source, part, owner, plane, fallback_owner, work
