"""Two-sided unlit spatial charts plus their untouched closed-geometry remainder."""

import numpy as np
from needle_composition import composite
from needle_group_reprojection import reproject
from needle_group_views import projection, render_view
from needle_spatial_planes import face_subset


def trim_view(view):
    y, x = np.nonzero(view["rgba"][..., 3])
    if not len(y):
        return view
    shape = np.array(view["rgba"].shape[:2])
    lo = np.maximum((y.min() - 1, x.min() - 1), 0)
    hi = np.minimum((y.max() + 2, x.max() + 2), shape)
    output = dict(view)
    for name, values in view.items():
        if isinstance(values, np.ndarray) and values.shape[:2] == tuple(shape):
            output[name] = values[lo[0] : hi[0], lo[1] : hi[1]].copy()
    step = (view["upper"] - view["lower"]) / shape[::-1]
    output["lower"] = view["lower"] + lo[::-1] * step
    output["upper"] = view["lower"] + hi[::-1] * step
    return output


def bake_patches(source, patches, texel_m=0.001):
    products = []
    for patch in patches:
        subset = face_subset(source, patch["faces"])
        pair = []
        for sign in (1, -1):
            direction = sign * patch["normal"]
            _, lower, upper = projection(subset["vertices"], direction)
            needed = max(16, int(np.ceil(np.max(upper - lower) / texel_m)))
            size = min(128, 2 ** int(np.ceil(np.log2(needed))))
            view = render_view(subset, direction, size, samples=4)
            view["source_depth"] = view["depth"]
            view["depth"] = view["rgba"][..., 3, None] * (sign * patch["offset_m"])
            pair.append(trim_view(view))
        products.append({"patch": patch, "views": pair})
    return products


def raw_image(view, samples):
    size = view["rgba"].shape[0]
    raw = view["raw_samples"].reshape(size, size, samples, samples, 9)
    raw = raw.transpose(0, 2, 1, 3, 4).reshape(size * samples, size * samples, 9)
    alpha = raw[..., 8, None]
    return dict(
        view,
        rgba=np.concatenate((raw[..., 3:6] * alpha, alpha), axis=-1),
        normal=raw[..., :3] * alpha,
        depth=raw[..., 7:8] * alpha,
        pixel_area_m2=view["pixel_area_m2"] / samples**2,
    )


def spatial_view(products, remainder, source, direction, size=64, samples=4):
    images = []
    for product in products:
        index = 0 if np.dot(product["patch"]["normal"], direction) >= 0 else 1
        view = product["views"][index]
        images.append(reproject(view, source, direction, size * samples, samples=1))
    if len(remainder):
        subset = face_subset(source, remainder)
        _, lower, upper = projection(source["vertices"], direction)
        view = render_view(subset, direction, size, (lower, upper), samples=samples)
        images.append(raw_image(view, samples))
    return composite(images, samples)
