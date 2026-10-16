"""Independent ideal-diffuse response to a controlled shared shadow receiver plane."""

import numpy as np

WIDTH, HEIGHT = 1280, 720
MATERIAL_BYTES = 384
LIGHT_ITEMS_FLOAT = 80
LIGHT_ITEM_FLOATS = 16


def reference(parameters):
    material = np.frombuffer(parameters, "<f4", 96)
    lights = np.frombuffer(parameters, "<f4", offset=MATERIAL_BYTES)
    if np.any(material[[2, 8, 9, 10, 13, 14, 15, 16, 17, 23, 24, 25, 27, 29, 31]] != 0):
        raise ValueError(
            "this oracle requires ideal diffuse material without other lobes"
        )
    if material[36] not in (0, 1) or np.any(material[37:40] != 0):
        raise ValueError(
            "this oracle requires an opaque/front-reflecting sheet control"
        )
    if not np.array_equal(lights[32:44], [1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0]):
        raise ValueError(
            "the declared planar receiver requires its fixed single shadow region"
        )
    x = (np.arange(WIDTH) + 0.5) * 2 / WIDTH - 1
    z = 1 - (np.arange(HEIGHT) + 0.5) * 2 / HEIGHT
    xx, zz = np.meshgrid(x, z)
    position = np.stack((xx, np.zeros_like(xx), zz), axis=-1)
    # The depth plane crosses the constant0.5 map between columns3/4 of eight.
    visibility = np.clip(8 * (xx * 0.5 + 0.5) - 3.5, 0, 1)
    total = np.zeros((*xx.shape, 3))
    for index in range(int(lights[0])):
        at = LIGHT_ITEMS_FLOAT + LIGHT_ITEM_FLOATS * index
        tint, place, beam, cone = lights[at : at + 16].reshape(4, 4).astype(float)
        toward = -beam[:3]
        attenuation = np.ones_like(xx)
        if tint[3] > 0.5:
            delta = place[:3] - position
            square = np.sum(delta**2, axis=-1)
            toward = delta / np.sqrt(square)[..., None]
            attenuation = np.clip(1 - (square * place[3] ** 2) ** 2, 0, 1) / square
        if tint[3] > 1.5:
            attenuation *= (
                np.clip((np.sum(beam[:3] * -toward, axis=-1) - cone[0]) * cone[1], 0, 1)
                ** 2
            )
        if tint[3] <= 0.5 and index + 1 == int(lights[2]):
            attenuation *= visibility if lights[3] > 0 else 1
        normal_cosine = np.maximum(toward[..., 1], 0)
        total += (
            material[4:7] * tint[:3] * (normal_cosine * attenuation)[..., None] / np.pi
        )
    return total
