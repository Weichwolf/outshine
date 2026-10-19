"""Shared opaque triangle-image diagnostic, not native PBR or GPU admission."""

import numpy as np
import trimesh
from PIL import Image, ImageDraw
from trimesh.ray.ray_triangle import RayMeshIntersector


def bilinear(image, uv):
    """Filter with zero exterior samples; caller supplies premultiplied fields."""
    height, width = image.shape[:2]
    p = uv * [width, height] - 0.5
    first = np.floor(p).astype(int)
    fraction = p - first
    result = np.zeros((len(uv), image.shape[2]))
    for x, y in ((0, 0), (1, 0), (0, 1), (1, 1)):
        index = first + [x, y]
        weight = (fraction[:, 0] if x else 1 - fraction[:, 0]) * (
            fraction[:, 1] if y else 1 - fraction[:, 1]
        )
        inside = ((index >= 0) & (index < [width, height])).all(1)
        result[inside] += (
            image[index[inside, 1], index[inside, 0]] * weight[inside, None]
        )
    return result


def prepare_source(source):
    """Prepare one immutable source's triangle index outside repeated view queries."""
    xyz, ids = source["positions"].astype(float), source["indices"]
    triangle = xyz[ids]
    normals = np.cross(triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0])
    mesh = trimesh.Trimesh(xyz, ids, process=False)
    # Construct the broad phase before measuring steady view queries.
    _ = mesh.triangles_tree
    return {
        "source": source,
        "mesh": mesh,
        "ray": RayMeshIntersector(mesh),
        "normal": normals,
    }


def intersect_source(source, origins, directions, *, prepared=None):
    data = prepare_source(source) if prepared is None else prepared
    if data["source"] is not source:
        raise ValueError("prepared index belongs to another source")
    mesh, normals = data["mesh"], data["normal"]
    # Locations deduplicate coincident front/back hits before culling. Keep all
    # triangle IDs, cull per ray, then choose the first valid front-facing depth.
    result = np.full(len(origins), np.inf)
    owner = np.full(len(origins), -1, np.int64)
    # Pin the triangle/R-tree implementation: Embree's iterative multi-hit query
    # can skip coincident front/back triangles even without location deduplication.
    # Bound temporary broad-phase candidate arrays independently of image size.
    for begin in range(0, len(origins), 256):
        o, d = origins[begin : begin + 256], directions[begin : begin + 256]
        faces, rays = data["ray"].intersects_id(
            o, d, return_locations=False, multiple_hits=True
        )
        denominator = np.einsum("ij,ij->i", normals[faces], d[rays])
        front = denominator < -1e-12
        rays, faces, denominator = rays[front], faces[front], denominator[front]
        plane = mesh.vertices[mesh.faces[faces, 0]]
        t = np.einsum("ij,ij->i", plane - o[rays], normals[faces]) / denominator
        keep = t > 0.1
        order = np.lexsort((faces[keep], t[keep], rays[keep]))
        rays, faces, t = (a[keep][order] for a in (rays, faces, t))
        first = np.r_[True, rays[1:] != rays[:-1]] if len(rays) else np.empty(0, bool)
        result[begin + rays[first]], owner[begin + rays[first]] = t[first], faces[first]
    return result, owner


def projected_bounds(positions, view):
    eye, basis, focal, width, height = view
    p = (positions.astype(float) - eye) @ basis.T
    if (p[:, 2] <= 0.1).any():
        raise ValueError("bounded diagnostic requires geometry in front of near plane")
    xy = [width / 2, height / 2] + focal * p[:, :2] / p[:, 2, None] * [1, -1]
    return np.stack((xy.min(0) - 2, xy.max(0) + 2))


def image_rays(view, box, width, height, supersample=2):
    eye, basis, focal, screen_width, screen_height = view
    x = box[0, 0] + (np.arange(width * supersample) + 0.5) / (
        width * supersample
    ) * np.ptp(box[:, 0])
    y = box[0, 1] + (np.arange(height * supersample) + 0.5) / (
        height * supersample
    ) * np.ptp(box[:, 1])
    xx, yy = np.meshgrid(x, y)
    direction = (
        np.column_stack(
            (
                (xx.ravel() - screen_width / 2) / focal,
                -(yy.ravel() - screen_height / 2) / focal,
                np.ones(xx.size),
            )
        )
        @ basis
    )
    return np.broadcast_to(eye, direction.shape), direction


def average_samples(values, width, height, supersample):
    return values.reshape(height, supersample, width, supersample, -1).mean((1, 3))


def relight(fields, sun, depth):
    sun = np.array(sun, float)
    sun /= np.linalg.norm(sun)
    light = 0.2 + 0.8 * np.maximum(fields["normal"] @ sun, 0)
    rgb = np.clip(fields["colour"] * light[:, None], 0, 1)
    return np.where(np.isfinite(depth)[:, None], rgb, 0)


def difference(reference, image):
    a, b = reference[:, :, 3], image[:, :, 3]
    union = (a > 0) | (b > 0)
    delta = image[:, :, :3] - reference[:, :, :3]
    opaque = (a >= 1 - 1e-6) & (b >= 1 - 1e-6)
    return {
        "reference_alpha_sum_px": float(a.sum()),
        "candidate_alpha_sum_px": float(b.sum()),
        "alpha_l1_px": float(abs(b - a).sum()),
        "lost_half_coverage_px": int(((a >= 0.5) & (b < 0.5)).sum()),
        "gained_half_coverage_px": int(((a < 0.5) & (b >= 0.5)).sum()),
        "premultiplied_rgb_union_rmse": float(np.sqrt(np.mean(delta[union] ** 2)))
        if union.any()
        else None,
        "premultiplied_rgb_roi_rmse": float(np.sqrt(np.mean(delta**2))),
        "common_opaque_pixels": int(opaque.sum()),
        "common_opaque_rgb_rmse": float(np.sqrt(np.mean(delta[opaque] ** 2)))
        if opaque.any()
        else None,
    }


def depth_difference(reference, candidate, covered):
    common = np.isfinite(reference) & np.isfinite(candidate) & covered
    delta = abs(candidate[common] - reference[common])
    return {
        "common_samples": int(common.sum()),
        "p95_depth_difference_m": float(np.percentile(delta, 95))
        if len(delta)
        else None,
        "max_depth_difference_m": float(delta.max()) if len(delta) else None,
    }


def panel(path, images, title):
    result = Image.new("RGB", (len(images[0]) * 384, 2 * 250), "white")
    draw = ImageDraw.Draw(result)
    for row, light in enumerate(images):
        for col, (name, pixels) in enumerate(light.items()):
            # Identical sky composite; metrics above use premultiplied RGB.
            rgb = pixels[:, :, :3] + (1 - pixels[:, :, 3:4]) * [0.45, 0.65, 0.85]
            im = Image.fromarray(np.uint8(np.clip(rgb, 0, 1) * 255 + 0.5))
            im.thumbnail((376, 210), Image.Resampling.NEAREST)
            # Enlarge small distant objects explicitly; original pixel data saved.
            factor = min(376 / im.width, 210 / im.height)
            im = im.resize(
                (int(im.width * factor), int(im.height * factor)),
                Image.Resampling.NEAREST,
            )
            result.paste(im, (col * 384 + 4, row * 250 + 36))
            draw.text(
                (col * 384 + 4, row * 250 + 4),
                f"{name}, light {row}: {title}",
                fill="black",
            )
    result.save(path)


def low_eye_view(positions, up, east, north, focal, distance, yaw, move):
    xyz = positions.astype(float)
    centre = (xyz.min(0) + xyz.max(0)) / 2
    angle = np.radians(yaw)
    eye = centre + distance * np.ptp(xyz, axis=0).max() * (
        np.cos(angle) * east + np.sin(angle) * north
    )
    eye += ((xyz @ up).min() + 1.7 - eye @ up) * up
    forward = centre - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, up)
    right /= np.linalg.norm(right)
    basis = np.stack((right, np.cross(right, forward), forward))
    return eye + move * right, basis, focal, 1280, 720
