"""Selected source contacts using existing Embree packets and corner interpolation."""

import time

import numpy as np
from needle_response import light_direction
from plant_atlas import projection
from scatter_scene import ScatterScene
from surface_normal_transfer import interpolate_normals


def prepare_source(path):
    if path.stat().st_size > 16 * 1024 * 1024:
        raise ValueError("selected frozen twig <=16MiB required")
    with np.load(path) as stored:
        source = {k: stored[k] for k in ("vertices", "faces", "normals")}
    if source["normals"].shape != (len(source["faces"]), 3, 3):
        raise ValueError("actual source corner normals required")
    if (
        not 1 <= len(source["faces"]) <= 131072
        or not np.isfinite(source["normals"]).all()
    ):
        raise ValueError("bounded nonempty frozen source and finite normals required")
    scene = ScatterScene(source["vertices"], source["faces"])
    for array in source.values():
        array.setflags(write=False)
    return source, scene


def camera_grid(source, view, size):
    view = light_direction(view)
    if type(size) is not int or size not in (16, 32, 64, 128, 256, 512):
        raise ValueError("explicit bounded power-of-two camera grid required")
    vertices = source["vertices"]
    frame, lower, upper, _ = projection(vertices, view)
    x, y = np.meshgrid(
        lower[0] + (np.arange(size) + 0.5) / size * (upper[0] - lower[0]),
        lower[1] + (np.arange(size) + 0.5) / size * (upper[1] - lower[1]),
    )
    heights = vertices @ view
    margin = max(float(np.ptp(heights)) * 0.05, 0.001)
    origins = (
        np.column_stack(
            (x.ravel(), y.ravel(), np.full(size * size, heights.max() + margin))
        )
        @ frame.T
    )
    return origins, -view, float(np.ptp(heights) + 2 * margin)


def camera_contacts(source, scene, view, size, packet=8192):
    if type(packet) is not int or not 1 <= packet <= 8192:
        raise ValueError("bounded primary packet required")
    began = time.perf_counter()
    origins, direction, length = camera_grid(source, view, size)
    hits, points = [], []
    for start in range(0, len(origins), packet):
        batch = origins[start : start + packet]
        ids, p, _, _ = scene.nearest(
            batch, np.broadcast_to(direction, batch.shape), length
        )
        hits.append(ids)
        points.append(p)
    faces = np.concatenate(hits)
    pixels = np.flatnonzero(faces >= 0)
    points = np.concatenate(points)[pixels]
    normals = (
        interpolate_normals(source, points, faces[pixels])
        if len(pixels)
        else np.empty((0, 3))
    )
    return {
        "size": size,
        "pixels": pixels,
        "faces": faces[pixels],
        "points": points,
        "normals": normals,
    }, {
        "rays": len(origins),
        "hits": len(pixels),
        "wall_s": time.perf_counter() - began,
        "primary_origin_array_bytes": origins.nbytes,
        "maximum_packet": packet,
    }


def direct_response(scene, contacts, direction, bias=2e-6, packet=8192):
    """Return cosine and unoccluded *positive* contacts; others remain unevaluated."""
    direction = light_direction(direction)
    if not np.isfinite(bias) or not 1e-7 <= bias <= 1e-4:
        raise ValueError("explicit finite metric ray bias1e-7..1e-4 required")
    if type(packet) is not int or not 1 <= packet <= 8192:
        raise ValueError("bounded direct-light packet required")
    cosine = np.maximum(contacts["normals"] @ direction, 0)
    demanded = np.flatnonzero(cosine > 0)
    visible = np.zeros(len(cosine), bool)
    length = scene.index.scale * 2 + 4 * bias
    before = dict(scene.work)
    began = time.perf_counter()
    for start in range(0, len(demanded), packet):
        selected = demanded[start : start + packet]
        origins = contacts["points"][selected] + bias * direction
        ids, _, _, _ = scene.nearest(
            origins, np.broadcast_to(direction, origins.shape), length
        )
        visible[selected] = ids < 0
    work = {key: scene.work[key] - before[key] for key in before}
    work.update(
        wall_s=time.perf_counter() - began,
        bias_m=bias,
        covered_contacts=len(cosine),
        skipped_nonpositive_contacts=len(cosine) - len(demanded),
    )
    return cosine, visible, work


def filtered_fields(contacts, cosine, visible, block):
    size = contacts["size"]
    if type(block) is not int or block < 1 or size % block:
        raise ValueError("positive disjoint integer source footprints required")
    if cosine.shape != visible.shape or cosine.shape != contacts["pixels"].shape:
        raise ValueError("same actual camera contacts required")
    count = size // block
    pixels = contacts["pixels"]
    coarse = (pixels // size // block) * count + pixels % size // block
    demanded = np.bincount(coarse[cosine > 0], minlength=count * count)
    unshadowed = np.bincount(coarse, weights=cosine, minlength=count * count)
    joint = np.bincount(coarse, weights=cosine * visible, minlength=count * count)
    fraction = np.divide(
        np.bincount(coarse, weights=visible, minlength=count * count),
        demanded,
        out=np.zeros(count * count),
        where=demanded > 0,
    )
    independent = unshadowed * fraction
    return np.stack((unshadowed, joint, independent)).reshape(3, count, count) / (
        block * block
    )
