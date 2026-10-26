"""Same orthographic depth view of the actual single-cell pool contact control."""

import numpy as np
from current_visibility import raster
from outshine_ref.contracts import Mesh
from outshine_ref.geometry.sampling import SurfaceSampler


def _basis():
    direction = np.array([0.6, -0.5, -0.65])
    direction /= np.linalg.norm(direction)
    right = np.cross(direction, [0, 0, 1])
    right /= np.linalg.norm(right)
    up = np.cross(right, direction)
    return np.array([right, up, direction])


def _view(body, terrain, faces, centre, *, span_m=5.0):
    vertices = np.vstack((body["vertices"], terrain))
    indices = np.vstack((body["faces"], faces + len(body["vertices"])))
    # Display a fixed local orthographic view; all scene triangles retain depth.
    basis = _basis()
    all_points = np.vstack((vertices, body["water_triangles"].reshape(-1, 3)))
    local = (all_points - centre) @ basis.T
    low, high = local[:, 2].min() - 1, local[:, 2].max() + 1
    projected = np.column_stack(
        (
            640 + 1280 / span_m * local[:, 0],
            360 - 1280 / span_m * local[:, 1],
            (high - local[:, 2]) / (high - low),
        )
    )
    z = local[:, 2] - low + 1
    depth, owner = raster(
        projected[: len(vertices)],
        z[: len(vertices)],
        indices,
        1280,
        720,
        with_faces=True,
    )
    palette = np.array(
        [
            [0.38, 0.38, 0.35],
            [0.65, 0.68, 0.67],
            [0.87, 0.83, 0.72],
            [0.74, 0.77, 0.75],
            [0.52, 0.55, 0.52],
        ]
    )
    colours = np.vstack(
        (palette[body["parts"]], np.tile([0.64, 0.45, 0.25], (len(faces), 1)))
    )
    p = vertices[indices]
    normal = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    normal /= np.linalg.norm(normal, axis=1)[:, None]
    light = np.array([-0.3, -0.5, 1.0])
    light /= np.linalg.norm(light)
    colours *= (0.35 + 0.65 * abs(normal @ light))[:, None]
    image = np.full((720, 1280, 3), [0.82, 0.85, 0.88])
    visible = owner >= 0
    image[visible] = colours[owner[visible]]
    water_faces = np.arange(len(all_points) - len(vertices)).reshape(-1, 3)
    water_depth = raster(
        projected[len(vertices) :], z[len(vertices) :], water_faces, 1280, 720
    )
    water_visible = water_depth > depth
    image[water_visible] = 0.45 * image[water_visible] + 0.55 * np.array(
        [0.10, 0.40, 0.62]
    )
    return image


def _section(axis, body, prior, original_faces, vertices, faces):
    t = np.linspace(0, 1, 512)
    xy = prior[0, :2] + t[:, None] * (prior[3, :2] - prior[0, :2])
    distance = t * np.linalg.norm(prior[3, :2] - prior[0, :2])
    for points, triangles, label in (
        (prior, original_faces, "provided current terrain"),
        (vertices, faces, "two-role contact candidate"),
    ):
        result = SurfaceSampler(Mesh(points, triangles)).sample(xy)
        if np.any(result.face_indices < 0):
            raise ValueError("pool section has unresolved terrain samples")
        axis.plot(distance, result.points_enu_m[:, 2], label=label)
    water = body["water_triangles"].reshape(-1, 3)
    result = SurfaceSampler(Mesh(water, np.arange(len(water)).reshape(-1, 3))).sample(
        xy
    )
    axis.plot(
        distance, result.points_enu_m[:, 2], label="unchanged water", color="#206a9a"
    )
    axis.set(
        title="Provided cell diagonal; adjacent edges still need update",
        xlabel="distance / m",
        ylabel="ENU Up / m",
    )
    axis.legend()


def display(body, prior, original_faces, vertices, faces, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    centre = np.r_[
        prior[:, :2].mean(axis=0), body["water_triangles"][:, :, 2].mean() - 0.5
    ]
    figure, axes = plt.subplots(1, 3, figsize=(17, 6))
    for axis, points, triangles, title in (
        (
            axes[0],
            prior,
            original_faces,
            "Same pool body + provided current terrain cell",
        ),
        (axes[1], vertices, faces, "Same body + conforming contact candidate"),
    ):
        axis.imshow(_view(body, points, triangles, centre))
        axis.set(title=title, xlabel="pixel x", ylabel="pixel y")
    _section(axes[2], body, prior, original_faces, vertices, faces)
    figure.suptitle(
        "1280×720 orthographic CPU depth / original single-cell control; neighbor updates pending; display transparency, no native/PBR image"
    )
    figure.tight_layout()
    figure.savefig(output / "pool-contact.png", dpi=150)
    plt.close(figure)
