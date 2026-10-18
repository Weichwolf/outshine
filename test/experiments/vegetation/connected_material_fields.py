"""Unlit original pigment/height and filterable slope moments in connected leaf UVs."""

import hashlib
import json

import numpy as np
from blade_surface import Blade
from leaf_sheet import average_blocks
from outshine_ref.vegetation.leaf_normals import resolve_moments
from raster_batch import rasterize


def repeated_image(image, uv):
    """Linear data with the original Blender image nodes' REPEAT boundary mode."""
    pixel = np.asarray(uv) * (image.shape[1], image.shape[0]) - 0.5
    lower = np.floor(pixel).astype(np.int64)
    fraction = pixel - lower
    result = np.zeros((*pixel.shape[:-1], image.shape[-1]))
    for dy in (0, 1):
        for dx in (0, 1):
            weight = (fraction[..., 0] if dx else 1 - fraction[..., 0]) * (
                fraction[..., 1] if dy else 1 - fraction[..., 1]
            )
            value = image[
                (lower[..., 1] + dy) % image.shape[0],
                (lower[..., 0] + dx) % image.shape[1],
            ]
            result += value * weight[..., None]
    return result


def material_contacts(recipe, colour, height, *, size=32, order=4):
    if type(size) is not int or size not in (16, 32, 64):
        raise ValueError("bounded material map size required")
    if type(order) is not int or order not in (2, 4, 8):
        raise ValueError("bounded midpoint capture order required")
    blade = Blade(**recipe["blade"])
    source = blade.mesh(*recipe["source_mesh"])
    t, s = source["parameters"].T
    xy = np.column_stack(
        (s * blade.width_profile(np.maximum(t, 0))[0], t * blade.length)
    )
    lower, upper = xy.min(axis=0), xy.max(axis=0)
    # Domain belongs to the saved geometry, independently of this material size.
    padding = (upper - lower) * 2 / (recipe["map_size"] - 4)
    lower, upper = lower - padding, upper + padding
    attributes = np.concatenate(
        (source["normals"].reshape(-1, 3, 3), source["parameters"][source["faces"]]),
        axis=-1,
    )
    mask, samples = rasterize(
        np.column_stack((xy, np.zeros(len(xy)))),
        source["faces"],
        attributes,
        lower,
        upper,
        size * order,
    )
    normals = samples[..., :3].astype(float)
    normals /= np.maximum(np.linalg.norm(normals, axis=-1, keepdims=True), 1e-12)
    if np.any(normals[..., 2][mask] <= 0.25):
        raise ValueError(
            "single positive object-Z slope chart cannot cover this source"
        )
    uv = np.stack(((samples[..., 4] + 1) / 2, samples[..., 3]), axis=-1)
    pigment = repeated_image(colour, uv)
    ridges = repeated_image(height[..., None], uv)[..., 0]
    slopes = normals[..., :2] / np.maximum(normals[..., 2, None], 0.25)
    return mask, slopes, pigment, ridges


def filtered_fields(mask, slopes, pigment, height, order):
    alpha = mask.astype(float)
    x, y = slopes[..., 0], slopes[..., 1]
    return {
        "colour": average_blocks(
            np.concatenate((pigment * alpha[..., None], alpha[..., None]), axis=-1),
            order,
        ),
        "slope_a": average_blocks(
            np.stack((x * alpha, y * alpha, x * x * alpha, alpha), axis=-1), order
        ),
        "slope_b": average_blocks(
            np.stack((x * y * alpha, y * y * alpha, height * alpha, alpha), axis=-1),
            order,
        ),
    }


def validate_fields(arrays):
    colour, a, b = (arrays[k] for k in ("colour", "slope_a", "slope_b"))
    size = len(colour)
    if size not in (16, 32, 64) or any(
        v.shape != (size, size, 4) or not np.isfinite(v).all() for v in (colour, a, b)
    ):
        raise ValueError("matching bounded finite material fields required")
    alpha = colour[..., 3]
    if (
        np.any((alpha < 0) | (alpha > 1))
        or not np.array_equal(a[..., 3], alpha)
        or not np.array_equal(b[..., 3], alpha)
        or np.any(colour[..., :3] < 0)
        or np.any(colour[..., :3] > alpha[..., None] + 1e-15)
        or np.any((b[..., 2] < 0) | (b[..., 2] > alpha + 1e-15))
    ):
        raise ValueError("same valid premultiplied coverage/colour/height required")
    values = np.stack(
        (a[..., 0], a[..., 1], a[..., 2], b[..., 0], b[..., 1], alpha * 0.55**4),
        axis=-1,
    )
    resolve_moments(values / np.where(alpha > 0, alpha, 1)[..., None])


def field_identity(arrays, description):
    keys = (
        "format",
        "carrier_recipe",
        "map_size",
        "midpoint_order",
        "material_identity",
        "source_uv",
        "fields",
        "scope",
    )
    digest = hashlib.sha256(
        json.dumps({k: description[k] for k in keys}, sort_keys=True).encode()
    )
    for key in ("colour", "slope_a", "slope_b"):
        digest.update(key.encode())
        digest.update(arrays[key].tobytes())
    return digest.hexdigest()


def build_material(recipe, colour, height, *, size=32, order=4, material_identity):
    contacts = material_contacts(recipe, colour, height, size=size, order=order)
    arrays = filtered_fields(*contacts, order)
    description = {
        "format": "connected-original-material-fields-1",
        "carrier_recipe": recipe,
        "map_size": size,
        "midpoint_order": order,
        "material_identity": material_identity,
        "source_uv": "u=(s+1)/2,v=t; linear REPEAT original images",
        "fields": "premultiplied colour, object-Z slope E[x,y,xx,xy,yy], height and coverage",
        "scope": "Finite UV capture, geometric slope moments only. Mean-height bump, unresolved bump variance and exact GGX convolution not claimed.",
    }
    validate_fields(arrays)
    for array in arrays.values():
        array.setflags(write=False)
    return arrays, description | {"identity": field_identity(arrays, description)}
