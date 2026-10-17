"""Joint finite thin-sheet R/T before filtering, using existing optical/ray kernels."""

import numpy as np
from needle_joint_contacts import direct_response
from needle_response import light_direction, source_samples
from outshine_ref.vegetation.leaf_lighting import leaf_diffuse_response
from outshine_ref.vegetation.leaf_optics import bounded_optics
from surface_normal_transfer import source_barycentrics


def contact_optics(source, points, face_ids):
    weights = source_barycentrics(source, points, face_ids)
    corners = source["optical"][source["faces"][face_ids]]
    return bounded_optics(np.sum(corners * weights[:, :, None], axis=1))


class SheetMeasure:
    def __init__(
        self, source, scene, view, directions, *, size=16, bias=2e-6, max_work=8_000_000
    ):
        if (
            source.get("surface_model") != "thin-sheet"
            or type(size) is not int
            or size not in (1, 4, 16, 64)
        ):
            raise ValueError(
                "explicit thin-sheet source and bounded native footprint required"
            )
        optical = bounded_optics(source["optical"])
        if (
            optical.shape != (len(source["vertices"]), 6)
            or not 1 <= len(directions) <= 8
        ):
            raise ValueError("per-source-vertex R/T and one to eight lights required")
        directions = np.array([light_direction(d) for d in directions])
        if type(max_work) is not int or not len(directions) <= max_work <= 8_000_000:
            raise ValueError("bounded explicit contact/light work required")
        self.source, self.scene, self.optical = source, scene, source["optical"]
        self.view, self.directions = (
            np.array(light_direction(view), copy=True),
            directions,
        )
        self.view.setflags(write=False)
        self.directions.setflags(write=False)
        self.size, self.bias = size, bias
        self.max_points, self.points = min(2_000_000, max_work // len(directions)), 0
        self.coverage = np.zeros(size * size)
        self.optical_sum = np.zeros((size * size, 6))
        self.cosines = np.zeros((len(directions), size * size, 2))
        self.joint = np.zeros((len(directions), size * size, 3))
        self.unshadowed, self.reflection = (
            np.zeros_like(self.joint),
            np.zeros_like(self.joint),
        )
        self.work = {
            "light_contact_products": 0,
            "reused_coaxial_active": 0,
            "skipped_zero_response": 0,
        }
        self.check(source, scene, self.view, size, bias, two_sided=True)

    def check(self, source, scene, view, size, bias, *, two_sided=False):
        if (
            source is not self.source
            or scene is not self.scene
            or size != self.size
            or bias != self.bias
            or not two_sided
            or source.get("surface_model") != "thin-sheet"
            or not np.array_equal(view, self.view)
            or scene.index.vertices is not source["vertices"]
            or scene.index.faces is not source["faces"]
            or source["optical"] is not self.optical
        ):
            raise ValueError(
                "same thin-sheet owner, camera, field, optical source and bias required"
            )
        if not np.isfinite(bias) or not 1e-7 <= bias <= 1e-4:
            raise ValueError("finite metric source-ray bias required")

    def light_visibility(self, points, normals, direction, response):
        active = np.max(response, axis=1) > 0
        visible = np.zeros(len(points), bool)
        self.work["skipped_zero_response"] += int((~active).sum())
        if np.array_equal(direction, self.view):
            visible[active] = True
            self.work["reused_coaxial_active"] += int(active.sum())
        elif active.any():
            facing = np.where(normals[active] @ direction < 0, -1, 1)
            _, visible[active], _ = direct_response(
                self.scene,
                {
                    "points": points[active],
                    "normals": normals[active] * facing[:, None],
                },
                direction,
                self.bias,
            )
        return visible

    def add(self, points, slots, mass, normals, *, face_ids=None):
        points, slots, face_ids = (
            np.asarray(points, float),
            np.asarray(slots),
            np.asarray(face_ids),
        )
        normals, mass = source_samples(normals, mass)
        if (
            points.shape != (len(normals), 3)
            or not np.isfinite(points).all()
            or len(points) > 8192
            or slots.shape != (len(points),)
            or slots.dtype.kind not in "iu"
            or slots.min(initial=0) < 0
            or slots.max(initial=-1) >= self.size**2
            or face_ids.shape != slots.shape
            or face_ids.dtype.kind not in "iu"
            or face_ids.min(initial=0) < 0
            or face_ids.max(initial=-1) >= len(self.source["faces"])
            or np.any(mass > 1 + 1e-12)
        ):
            raise ValueError("bounded same-source face/contact packet required")
        active = mass > 0
        points, slots, mass, normals, face_ids = (
            a[active] for a in (points, slots, mass, normals, face_ids)
        )
        if self.points + len(points) > self.max_points:
            raise ValueError("contact/light demand exceeded before optical/ray work")
        if not len(points):
            return
        optical = contact_optics(self.source, points, face_ids)
        self.points += len(points)
        self.work["light_contact_products"] += len(points) * len(self.directions)
        self.coverage += np.bincount(slots, weights=mass, minlength=self.size**2)
        for channel in range(6):
            self.optical_sum[:, channel] += np.bincount(
                slots, weights=mass * optical[:, channel], minlength=self.size**2
            )
        for light, direction in enumerate(self.directions):
            response = leaf_diffuse_response(optical, normals, self.view, direction)
            visible = self.light_visibility(points, normals, direction, response)
            signed = np.where(normals @ self.view < 0, -1, 1) * (normals @ direction)
            for side in range(2):
                weights = mass * np.maximum(signed * (1 if side == 0 else -1), 0)
                self.cosines[light, :, side] += np.bincount(
                    slots, weights=weights, minlength=self.size**2
                )
            for channel in range(3):
                values = mass * response[:, channel]
                for field, weights in [
                    (self.unshadowed, values),
                    (self.joint, values * visible),
                    (self.reflection, values * visible * (signed >= 0)),
                ]:
                    field[light, :, channel] += np.bincount(
                        slots, weights=weights, minlength=self.size**2
                    )

    def separate_unshadowed(self):
        mean = np.divide(
            self.optical_sum,
            self.coverage[:, None],
            out=np.zeros_like(self.optical_sum),
            where=self.coverage[:, None] > 0,
        )
        return (
            self.cosines[..., :1] * mean[None, :, :3]
            + self.cosines[..., 1:] * mean[None, :, 3:]
        ) / np.pi
