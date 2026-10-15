"""Independent-direction view-card coverage and depth against the closed oracle."""

import numpy as np
from leaf_sheet import sample_image
from needle_group_views import projection
from plant_depth import depth_mesh
from raster_batch import rasterize


def reproject(view, source, direction, size=64, samples=4, grid=1):
    frame, lower, upper = projection(source["vertices"], direction)
    image = np.concatenate(
        (view["rgba"], view["normal"], view["roughness_fourth"], view["depth"]), axis=-1
    )
    mesh = depth_mesh(
        image, view["frame"], view["lower"], view["upper"], grid, anchored=False
    )
    corners, faces, uv = mesh["vertices"] @ frame, mesh["faces"], mesh["uv"]
    width = size * samples
    attributes = np.concatenate((uv[faces], corners[faces, 2, None]), axis=-1)
    covered, data = rasterize(corners, faces, attributes, lower, upper, width)
    coordinates = data[..., :2]
    rgba = sample_image(view["rgba"], coordinates) * covered[..., None]
    normals = (
        sample_image(view["normal"], coordinates) @ view["frame"].T @ frame
    ) * covered[..., None]
    depth = data[..., 2, None] * rgba[..., 3, None]

    def reduce(data):
        return data.reshape(size, samples, size, samples, -1).mean((1, 3))

    return {
        "rgba": reduce(rgba),
        "normal": reduce(normals),
        "depth": reduce(depth),
        "frame": frame,
        "lower": lower,
        "upper": upper,
        "pixel_area_m2": float(np.prod(upper - lower) / size**2),
    }


def compare_images(reference, candidate):
    a, b = reference["rgba"][..., 3], candidate["rgba"][..., 3]
    total = a.sum()
    covered = (a > 0.05) & (b > 0.05)
    da = reference["depth"][..., 0] / np.maximum(a, 1e-20)
    db = candidate["depth"][..., 0] / np.maximum(b, 1e-20)
    return {
        "coverage_area_relative_error": float(b.sum() / total - 1),
        "coverage_l1_relative": float(abs(a - b).sum() / total),
        "depth_rmse_m": float(np.sqrt(np.mean((da[covered] - db[covered]) ** 2)))
        if covered.any()
        else None,
    }
