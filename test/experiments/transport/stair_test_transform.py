"""Rigid test inputs: evaluate each original XY once and retain exact mesh sharing."""

import numpy as np


def rigid_coordinates(angle, translation):
    rotation = np.array(
        [[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]]
    )
    images = {}

    def coordinates(value):
        result = np.array(value, dtype=float, copy=True)
        for point in result.reshape(-1, result.shape[-1]):
            original = tuple(point[:2])
            if original not in images:
                images[original] = np.array(original) @ rotation.T + translation
            point[:2] = images[original]
        return result.tolist()

    return coordinates, rotation


def assert_shared_vertices(test, original_cuts, transformed_cuts):
    images, repeats = {}, 0
    for original, transformed in zip(original_cuts, transformed_cuts, strict=True):
        for before, after in zip(
            original["triangles"], transformed["triangles"], strict=True
        ):
            for source, target in zip(before["enu_m"], after["enu_m"], strict=True):
                key, image = tuple(source), tuple(target)
                if key in images:
                    test.assertEqual(images[key], image)
                    repeats += 1
                images[key] = image
    test.assertGreater(repeats, 0)
