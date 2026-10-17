"""Stream joint light visibility/cosine before filtering on shared visible contacts."""

import numpy as np
from needle_joint_contacts import direct_response
from needle_response import light_direction, source_samples


class JointMeasure:
    def __init__(
        self, source, scene, view, directions, *, size=16, bias=2e-6, max_work=8_000_000
    ):
        directions = np.asarray(directions, float)
        if (
            directions.ndim != 2
            or directions.shape[1:] != (3,)
            or not 1 <= len(directions) <= 128
        ):
            raise ValueError("one to128bounded unit light queries required")
        directions = np.array([light_direction(v) for v in directions])
        if type(size) is not int or size not in (1, 4, 16, 64):
            raise ValueError("bounded native footprint required")
        if type(max_work) is not int or not len(directions) <= max_work <= 64_000_000:
            raise ValueError("explicit bounded contact/light product work required")
        self.source, self.scene = source, scene
        self.view = np.array(light_direction(view), copy=True)
        self.directions = directions
        self.view.setflags(write=False)
        self.directions.setflags(write=False)
        self.size, self.bias = size, bias
        self.max_points = min(2_000_000, max_work // len(directions))
        self.points = 0
        self.coverage = np.zeros(size * size)
        self.unshadowed = np.zeros((len(directions), size * size))
        self.joint = np.zeros_like(self.unshadowed)
        self.visible_mass = np.zeros_like(self.unshadowed)
        self.positive_mass = np.zeros_like(self.unshadowed)
        self.work = {"light_contact_products": 0, "reused_coaxial_positive": 0}
        self.check(source, scene, self.view, size, bias)

    def check(self, source, scene, view, size, bias, *, two_sided=False):
        if (
            source is not self.source
            or scene is not self.scene
            or size != self.size
            or bias != self.bias
            or not np.array_equal(view, self.view)
            or scene.index.vertices is not source["vertices"]
            or scene.index.faces is not source["faces"]
            or two_sided
        ):
            raise ValueError(
                "same immutable source, camera, field and metric bias required"
            )
        if not np.isfinite(bias) or not 1e-7 <= bias <= 1e-4:
            raise ValueError("explicit finite source-ray bias required")

    def add(self, points, slots, mass, normals, *, face_ids=None):
        points, slots = np.asarray(points, float), np.asarray(slots)
        normals, mass = source_samples(normals, mass)
        if (
            points.shape != (len(normals), 3)
            or not np.isfinite(points).all()
            or slots.shape != (len(points),)
            or slots.dtype.kind not in "iu"
            or len(points) > 8192
            or slots.min(initial=0) < 0
            or slots.max(initial=-1) >= self.size**2
            or np.any(mass > 1 + 1e-12)
        ):
            raise ValueError(
                "bounded same-field source contacts required before light work"
            )
        if np.any(mass == 0):
            active = mass > 0
            points, slots, mass, normals = (
                v[active] for v in (points, slots, mass, normals)
            )
        if self.points + len(points) > self.max_points:
            raise ValueError("contact/light work exceeded before light queries")
        if not len(points):
            return
        self.points += len(points)
        self.work["light_contact_products"] += len(points) * len(self.directions)
        self.coverage += np.bincount(slots, weights=mass, minlength=self.size**2)
        for index, direction in enumerate(self.directions):
            if np.array_equal(direction, self.view):
                cosine = np.maximum(normals @ direction, 0)
                clear = cosine > 0
                self.work["reused_coaxial_positive"] += int(clear.sum())
            else:
                cosine, clear, _ = direct_response(
                    self.scene,
                    {"points": points, "normals": normals},
                    direction,
                    self.bias,
                )
            for output, weights in [
                (self.unshadowed, mass * cosine),
                (self.joint, mass * cosine * clear),
                (self.visible_mass, mass * clear),
                (self.positive_mass, mass * (cosine > 0)),
            ]:
                output[index] += np.bincount(
                    slots, weights=weights, minlength=self.size**2
                )

    def independent(self):
        fraction = np.divide(
            self.visible_mass,
            self.positive_mass,
            out=np.zeros_like(self.visible_mass),
            where=self.positive_mass > 0,
        )
        return self.unshadowed * fraction
