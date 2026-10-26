"""Only requested real cached-axis needles using existing physical prototypes."""

from dataclasses import replace

import numpy as np
from outshine_ref.vegetation import NeedlePattern, Profile
from outshine_ref.vegetation.axis_surface import axis_mesh
from spruce_growth_product import support

MAX_NEEDLES = 1024
MAX_TRIANGLES = 250_000


def select_axes(axes):
    orders = np.diff(axes["path_offsets"]).astype(np.int64) - 2
    result = []
    for order in (1, 2, 3):
        possible = np.flatnonzero(
            (orders == order) & (axes["needle_counts"] <= MAX_NEEDLES)
        )
        ranked = possible[np.argsort(axes["needle_counts"][possible], kind="stable")]
        if len(ranked) < 3:
            raise ValueError("three real bounded axes per needle order required")
        result.extend(
            int(ranked[round(q * (len(ranked) - 1))]) for q in (0.05, 0.5, 0.95)
        )
    if (
        len(set(result)) != 9
        or sum(int(axes["needle_counts"][i]) for i in result) > 6000
    ):
        raise ValueError("nine distinct real requests and6000total needles required")
    return result


def physical_axis(graph, axes, physical, models, index, *, needle_range=None):
    axis = support(axes, index)
    order = len(axis.path)
    pattern = NeedlePattern.from_recipe(physical["patterns"][order])
    count = int(axes["needle_counts"][index])
    triangle_bound = count * max(len(mesh["faces"]) for mesh, _ in models)
    triangle_bound += count // pattern.group_size * (36 if pattern.short_length else 0)
    if not 0 < count <= MAX_NEEDLES or triangle_bound > MAX_TRIANGLES:
        raise ValueError("requested needle/triangle budgets must precede detail")
    lo, hi = (0, count) if needle_range is None else needle_range
    if (
        type(lo) is not int
        or type(hi) is not int
        or not 0 <= lo < hi <= count
        or lo % pattern.group_size
        or hi % pattern.group_size
    ):
        raise ValueError("explicit whole-group subset inside requested twig required")
    shape = graph["nodes"][order + 1]["shape"]
    profile = Profile(np.array(shape["radius"]["table"]), shape["radius"]["kind"])
    radii = profile.evaluate(np.array((0.0, 1.0))) * axes["uniform_scales"][index]
    local = replace(axis, origin=np.zeros(3), frame=np.eye(3))
    details = pattern.detail(local, radii, graph["seed"], count)
    first = int(axes["first_needle_ids"][index]) + lo
    global_ids = np.arange(first, first + hi - lo, dtype=np.uint64)
    variants = (global_ids % len(models) + graph["seed"] % len(models)) % len(models)
    vertices, faces, normals, colours, leaf, offset = [], [], [], [], [], 0

    def append(mesh, colour, positions, frames, scales, is_leaf):
        nonlocal offset
        n = len(positions)
        if not n:
            return
        v = (
            np.einsum("nij,vj->nvi", frames, mesh["vertices"]) * scales[:, None, None]
            + positions[:, None]
        )
        f = (
            mesh["faces"][None]
            + np.arange(n)[:, None, None] * len(mesh["vertices"])
            + offset
        )
        normal = np.einsum("nij,fkj->nfki", frames, mesh["normals"].reshape(-1, 3, 3))
        vertices.append(v.reshape(-1, 3))
        faces.append(f.reshape(-1, 3))
        normals.append(normal.reshape(-1, 3, 3))
        colours.append(np.broadcast_to(colour, (n, *colour.shape)).reshape(-1, 3, 3))
        leaf.append(np.full(n * len(mesh["faces"]), is_leaf, bool))
        offset += n * len(mesh["vertices"])

    for variant, (mesh, colour) in enumerate(models):
        mask = variants == variant
        append(
            mesh,
            colour,
            details["origins"][lo:hi][mask],
            details["frames"][lo:hi][mask],
            details["scales"][lo:hi][mask],
            True,
        )
    if pattern.short_length:
        mesh = axis_mesh(pattern.short_length, 0, 0.0002, 0.00014, 2, 6)
        colour = np.broadcast_to((0.14, 0.105, 0.037), (len(mesh["faces"]), 3, 3))
        append(
            mesh,
            colour,
            details["short_origins"][
                lo // pattern.group_size : hi // pattern.group_size
            ],
            details["short_frames"][
                lo // pattern.group_size : hi // pattern.group_size
            ],
            np.ones((hi - lo) // pattern.group_size),
            False,
        )
    source = {
        "vertices": np.concatenate(vertices),
        "faces": np.concatenate(faces),
        "normals": np.concatenate(normals),
        "colour": np.concatenate(colours),
        "leaf": np.concatenate(leaf),
    }
    if len(source["faces"]) > triangle_bound:
        raise ValueError(
            "actual prototype topology must respect admitted triangle bound"
        )
    return (
        source,
        details,
        {
            "axis_id": index,
            "path": axis.path,
            "order": order,
            "length_m": axis.length,
            "bend_rad": axis.bend,
            "needles": hi - lo,
            "axis_needles": count,
            "first_needle_id": first,
            "variants": np.bincount(
                variants.astype(int), minlength=len(models)
            ).tolist(),
            "triangles": len(source["faces"]),
            "geometry_array_bytes": sum(v.nbytes for v in source.values()),
            "detail_array_bytes": sum(v.nbytes for v in details.values()),
        },
    )


def screen_span(geometry, axes, index, scene):
    world = geometry["vertices"] @ axes["rotations"][index].T + axes["origins"][index]
    eye, target = np.array(scene["camera"]), np.array(scene["target"])
    forward = target - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, (0, 0, 1))
    right /= np.linalg.norm(right)
    up = np.cross(right, forward)
    view = (world - eye) @ np.column_stack((right, up, forward))
    if np.any(view[:, 2] <= 0):
        raise ValueError("actual twig must stay in front of the declared camera")
    focal = scene["resolution"][1] / (2 * np.tan(np.deg2rad(scene["fov_y"]) / 2))
    return (np.ptp(focal * view[:, :2] / view[:, 2, None], axis=0)).tolist()


def local_windows(axes, windows, index):
    mask = windows["axis_ids"] == index
    result = {k: v[mask].copy() for k, v in windows.items()}
    rotation = axes["rotations"][index]
    result["origins"] = (result["origins"] - axes["origins"][index]) @ rotation
    result["frames"] = np.einsum("ji,njk->nik", rotation, result["frames"])
    count = int(axes["needle_counts"][index])
    first = int(axes["first_needle_ids"][index])
    if (
        int(result["needle_counts"].sum()) != count
        or int(result["first_needle_ids"][0]) != first
    ):
        raise ValueError("matching complete requested needle owner interval required")
    return result
