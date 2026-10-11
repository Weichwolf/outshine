"""Root-preserving planar leaf charts with coverage-weighted material attributes."""

from dataclasses import dataclass

import numpy as np
from PIL import Image
from shoot_plan import normal_frame


def rasterize(vertices, faces, attributes, lower, upper, size):
    """Orthographic two-sided triangles, pixel centres, nearest positive Z layer."""
    projected = (vertices[:, :2] - lower) / (upper - lower) * size
    depth = np.full((size, size), -np.inf)
    output = np.zeros((size, size, attributes.shape[-1]), dtype=np.float32)
    covered = np.zeros((size, size), dtype=bool)
    for index, face in enumerate(faces):
        triangle = projected[face]
        first = np.maximum(np.floor(triangle.min(axis=0)).astype(int), 0)
        last = np.minimum(np.ceil(triangle.max(axis=0)).astype(int), size)
        if np.any(first >= last):
            continue
        ab, ac = triangle[1] - triangle[0], triangle[2] - triangle[0]
        determinant = ab[0] * ac[1] - ab[1] * ac[0]
        if abs(determinant) < 1e-12:
            continue
        x, y = np.meshgrid(
            np.arange(first[0], last[0]) + 0.5, np.arange(first[1], last[1]) + 0.5
        )
        p = np.stack((x, y), axis=-1) - triangle[0]
        b = (p[..., 0] * ac[1] - p[..., 1] * ac[0]) / determinant
        c = (ab[0] * p[..., 1] - ab[1] * p[..., 0]) / determinant
        weights = np.stack((1 - b - c, b, c), axis=-1)
        z = weights @ vertices[face, 2]
        target = np.s_[first[1] : last[1], first[0] : last[0]]
        valid = np.all(weights >= -1e-10, axis=-1) & (z >= depth[target])
        depth[target][valid] = z[valid]
        covered[target][valid] = True
        output[target][valid] = (weights @ attributes[index])[valid]
    return covered, output


def average_blocks(values, factor=2):
    height, width = values.shape[:2]
    if height % factor or width % factor:
        raise ValueError("filter factor must divide image dimensions")
    return values.reshape(
        height // factor, factor, width // factor, factor, *values.shape[2:]
    ).mean(axis=(1, 3))


def sample_image(image, uv):
    pixel = np.asarray(uv) * np.array((image.shape[1], image.shape[0])) - 0.5
    lower = np.floor(pixel).astype(int)
    weight = pixel - lower
    upper = lower + 1
    lower = np.clip(lower, 0, np.array((image.shape[1] - 1, image.shape[0] - 1)))
    upper = np.clip(upper, 0, np.array((image.shape[1] - 1, image.shape[0] - 1)))
    bottom = (
        image[lower[..., 1], lower[..., 0]] * (1 - weight[..., 0, None])
        + image[lower[..., 1], upper[..., 0]] * weight[..., 0, None]
    )
    top = (
        image[upper[..., 1], lower[..., 0]] * (1 - weight[..., 0, None])
        + image[upper[..., 1], upper[..., 0]] * weight[..., 0, None]
    )
    return bottom * (1 - weight[..., 1, None]) + top * weight[..., 1, None]


def projection_bound(blade, normal, divisions=256):
    t, s = np.meshgrid(
        np.linspace(0, 1, divisions + 1),
        np.linspace(-1, 1, divisions + 1),
        indexing="ij",
    )
    points = blade.evaluate(t, s, np.where(s < 0, -1, 1))[0]
    root = np.array((0.0, -blade.petiole, 0.0))
    measured = np.abs((points - root) @ normal).max()
    warp = max(blade.widest / (1 - blade.widest), (1 - blade.widest) / blade.widest)
    width_derivative = (
        blade.width
        / 2
        * (
            np.pi * warp * (1 - blade.base_fill + blade.lobe_depth * blade.lobes)
            + blade.base_fill
        )
    )
    longitudinal = (
        blade.length
        + width_derivative * (1 + abs(blade.fold))
        + blade.width / 2 * (abs(blade.bend) + abs(blade.twist)) * (1 + abs(blade.fold))
    )
    transverse = blade.width / 2 * (1 + blade.fold_power * abs(blade.fold))
    if blade.petiole:
        petiole = blade.evaluate(
            -blade.petiole / blade.length, s[0], np.where(s[0] < 0, -1, 1)
        )[0]
        measured = max(measured, np.abs((petiole - root) @ normal).max())
    return float(
        measured + longitudinal / (2 * divisions) + transverse / divisions + 1e-12
    )


@dataclass(frozen=True)
class LeafSheet:
    mesh: dict
    colour: np.ndarray
    normal: np.ndarray
    frame: np.ndarray
    origin: np.ndarray
    lower: np.ndarray
    upper: np.ndarray
    displacement_bound_m: float

    def coordinates(self, vertices):
        projected = (vertices - self.origin) @ self.frame
        return (projected[:, :2] - self.lower) / (self.upper - self.lower)

    def save(self, directory, stem):
        paths = {}
        for name, data in (("colour", self.colour), ("normal", self.normal)):
            filename = f"{stem}-{name}.png"
            encoded = np.rint(np.clip(data, 0, 1) * 255).astype(np.uint8)
            Image.fromarray(np.flipud(encoded)).save(directory / filename)
            paths[name] = filename
        return paths


def make_sheet(blade, variant=0, size=256, fit="anchored"):
    if type(size) is not int or size < 32 or size > 1024 or size & (size - 1):
        raise ValueError("power-of-two chart size from 32 to 1024 required")
    if fit not in ("anchored", "flat"):
        raise ValueError("declared sheet fitting method required")
    source = blade.mesh(64, across=2)
    root = np.array((0.0, -blade.petiole, 0.0))
    triangles = source["vertices"][source["faces"]]
    area = np.linalg.norm(
        np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]),
        axis=-1,
    )
    points = triangles - root
    covariance = np.einsum("nki,nkj,n->ij", points, points, area)
    normal = (
        np.linalg.eigh(covariance)[1][:, 0]
        if fit == "anchored"
        else np.array((0.0, 0.0, 1.0))
    )
    normal *= 1 if normal[2] >= 0 else -1
    forward = np.array((0.0, 1.0, 0.0))
    forward -= np.dot(forward, normal) * normal
    if np.linalg.norm(forward) < 1e-8:
        raise ValueError("folded-over leaf requires spatial geometry")
    frame = normal_frame(forward, normal)
    projected = (source["vertices"] - root) @ frame
    oriented = (
        np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0])
        @ normal
    )
    if np.any(oriented <= 0):
        raise ValueError("folded-over leaf requires spatial geometry")
    lower, upper = projected[:, :2].min(axis=0), projected[:, :2].max(axis=0)
    padding = (upper - lower) * 2 / (size - 4)
    lower, upper = lower - padding, upper + padding
    parameters = source["parameters"][source["faces"]]
    attributes = np.concatenate(
        (source["normals"].reshape(-1, 3, 3) @ frame, parameters), axis=-1
    )
    covered, samples = rasterize(
        projected, source["faces"], attributes, lower, upper, size * 2
    )
    normals = samples[..., :3]
    normals /= np.maximum(np.linalg.norm(normals, axis=-1, keepdims=True), 1e-12)
    vein = np.clip((0.014 - np.abs(samples[..., 4] / 2)) / 0.012, 0, 1)
    shade = (variant % 8 + 0.5) / 8
    base = (
        np.array((0.022, 0.075, 0.009)) * (1 - shade)
        + np.array((0.11, 0.23, 0.025)) * shade
    )
    colour = base + vein[..., None] * (np.array((0.17, 0.26, 0.046)) - base)
    alpha = covered.astype(float)[..., None]
    colour = average_blocks(np.concatenate((colour * alpha, alpha), axis=-1))
    encoded = (normals + 1) / 2
    normal_map = average_blocks(np.concatenate((encoded * alpha, alpha), axis=-1))
    xy = np.array(
        (
            (lower[0], lower[1]),
            (upper[0], lower[1]),
            (upper[0], upper[1]),
            (lower[0], upper[1]),
        )
    )
    vertices = root + xy @ frame[:, :2].T
    mesh = {
        "vertices": vertices,
        "faces": np.array(((0, 1, 2), (0, 2, 3)), dtype=np.int32),
        "normals": np.broadcast_to(frame[:, 2], (6, 3)).copy(),
        "uv": np.array(((0, 0), (1, 0), (1, 1), (0, 1)), dtype=float),
    }
    return LeafSheet(
        mesh,
        colour,
        normal_map,
        frame,
        root,
        lower,
        upper,
        projection_bound(blade, frame[:, 2]),
    )
