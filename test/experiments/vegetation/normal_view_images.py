"""Independent clamped images as a Blender reference for texture-array layers."""

import bpy
import numpy as np


class ViewImages:
    def __init__(self, view):
        if type(view) is not int or not 0 <= view < 64:
            raise ValueError("one bounded source view required")
        self.view = view
        self.images = []

    def __call__(self, values):
        values = np.asarray(values, np.float32)
        if (
            values.ndim != 4
            or values.shape[1:] != (32, 32, 3)
            or not 1 <= len(values) <= 64
            or self.view >= len(values)
        ):
            raise ValueError("bounded source-view field required")
        selected = values[self.view]
        if not np.isfinite(selected).all():
            raise ValueError("finite demanded source view required")
        pixels = np.ones((32, 32, 4), np.float32)
        pixels[..., :3] = selected
        image = bpy.data.images.new(
            "independent positive normal view",
            width=32,
            height=32,
            alpha=True,
            float_buffer=True,
        )
        image.colorspace_settings.name = "Non-Color"
        image.alpha_mode = "CHANNEL_PACKED"
        image.use_half_precision = False
        image.pixels.foreach_set(pixels.ravel())
        image.update()
        self.images.append(image)
        return image

    @property
    def float_payload_bytes(self):
        return sum(image.size[0] * image.size[1] * 4 * 4 for image in self.images)
