"""The existing mass-construction camera with source-informed opaque GGX colours."""

import numpy as np
from building_mass_material_image import evaluate
from current_visibility import raster
from PIL import Image, ImageDraw
from urban_parent_light import display


def frame(original, candidate):
    forward = np.array([-0.45, 0.55, -0.7])
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, [0, 0, 1])
    right /= np.linalg.norm(right)
    basis = np.array([right, np.cross(right, forward), forward])
    points = np.vstack((original["positions"], candidate["positions"])) @ basis.T
    low, high = points.min(0), points.max(0)
    scale = max((high[0] - low[0]) / 1280, (high[1] - low[1]) / 720) / 0.94
    return basis, low, (low + high) / 2, scale


def samples(source, camera):
    basis, low, centre, scale = camera
    p = source["positions"] @ basis.T
    z = p[:, 2] - low[2] + 1
    projected = np.column_stack(
        (
            640 + (p[:, 0] - centre[0]) / scale,
            360 - (p[:, 1] - centre[1]) / scale,
            0.1 / z,
        )
    )
    depth, faces = raster(projected, z, source["indices"], 1280, 720, with_faces=True)
    y, x = np.nonzero(faces >= 0)
    local = np.column_stack(
        (
            centre[0] + (x + 0.5 - 640) * scale,
            centre[1] - (y + 0.5 - 360) * scale,
            0.1 / depth[y, x] + low[2] - 1,
        )
    )
    return {
        "hit": (faces >= 0).ravel(),
        "faces": faces[y, x],
        "points": local @ basis,
        "outgoing": np.tile(-basis[2], (len(x), 1)),
    }


def construction(original, candidate, fields, appearance, product, tables, output):
    camera = frame(original, candidate)
    contact = Image.new("RGB", (2560, 746))
    draw = ImageDraw.Draw(contact)
    reports = {}
    for column, (name, mesh) in enumerate(
        (("original", original), ("material", candidate))
    ):
        image, reports[name] = evaluate(
            mesh,
            original,
            fields,
            appearance,
            product,
            None,
            np.array([[0, 0], [1280, 720]]),
            tables,
            parent=column == 1,
            diagnostic=samples(mesh, camera),
        )
        rgb = display(image["C"] + 0.1 * image["T"])
        contact.paste(Image.fromarray(rgb), (1280 * column, 26))
        draw.text(
            (1280 * column + 8, 6),
            name + " / source-model GGX, geometric normals, no shadows",
            fill="white",
        )
    contact.save(output / "construction.png")
    return reports
