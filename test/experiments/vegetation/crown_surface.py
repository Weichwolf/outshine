"""Direct nested crown envelopes, not a representation of individual leaves."""

from dataclasses import dataclass, field

import numpy as np


def face_axes(axis, sign):
    normal = np.eye(3)[axis] * sign
    first = np.eye(3)[(axis + 1) % 3]
    return normal, first, np.cross(normal, first)


def cube_mesh(level):
    if not isinstance(level, (int, np.integer)) or not 0 <= level <= 7:
        raise ValueError("integer subdivision level between zero and seven required")
    divisions = 1 << level
    coordinates = np.linspace(-1, 1, divisions + 1)
    u, v = np.meshgrid(coordinates, coordinates, indexing="ij")
    parameters = []
    for axis in range(3):
        for sign in (-1, 1):
            normal, first, second = face_axes(axis, sign)
            parameters.append(normal + u[..., None] * first + v[..., None] * second)
    parameters, indices = np.unique(
        np.stack(parameters).reshape(-1, 3), axis=0, return_inverse=True
    )
    indices = indices.reshape(6, divisions + 1, divisions + 1)
    a, b = indices[:, :-1, :-1], indices[:, 1:, :-1]
    c, d = indices[:, 1:, 1:], indices[:, :-1, 1:]
    faces = np.stack((np.stack((a, b, c), -1), np.stack((a, c, d), -1)), -2)
    return parameters, faces.reshape(-1, 3).astype(np.int32)


@dataclass(frozen=True)
class Crown:
    radii: tuple
    centre: tuple
    seed: int
    amplitudes: tuple = (0.12, 0.045, 0.014)
    frequencies: tuple = (3.5, 9.0, 20.0)
    directions: np.ndarray = field(init=False, repr=False, compare=False)
    phases: np.ndarray = field(init=False, repr=False, compare=False)

    def __post_init__(self):
        radii, centre = np.asarray(self.radii), np.asarray(self.centre)
        if radii.shape != (3,) or centre.shape != (3,):
            raise ValueError("three radii and centre coordinates required")
        if not np.all(np.isfinite([radii, centre])) or np.any(radii <= 0):
            raise ValueError("finite centre and positive finite radii required")
        if not self.amplitudes or len(self.amplitudes) != len(self.frequencies):
            raise ValueError("one frequency per displacement band required")
        if not all(np.isfinite(v) and v >= 0 for v in self.amplitudes):
            raise ValueError("nonnegative finite amplitudes required")
        if not all(np.isfinite(v) and v > 0 for v in self.frequencies):
            raise ValueError("positive finite frequencies required")
        if sum(self.amplitudes) >= 0.5:
            raise ValueError("radial displacement must remain strictly positive")
        random = np.random.default_rng(self.seed)
        directions = random.normal(size=(len(self.amplitudes), 3))
        directions /= np.linalg.norm(directions, axis=1)[:, None]
        phases = random.uniform(0, 2 * np.pi, len(self.amplitudes))
        for name, values in (("directions", directions), ("phases", phases)):
            values.setflags(write=False)
            object.__setattr__(self, name, values)
        for name, values in (("radii", radii), ("centre", centre)):
            object.__setattr__(self, name, tuple(float(v) for v in values))
        for name in ("amplitudes", "frequencies"):
            object.__setattr__(self, name, tuple(float(v) for v in getattr(self, name)))

    def band_count(self, bands):
        count = len(self.amplitudes) if bands is None else bands
        if not isinstance(count, (int, np.integer)) or not 0 <= count <= len(
            self.amplitudes
        ):
            raise ValueError("bounded integer displacement band count required")
        return count

    def evaluate(self, parameters, bands=None):
        bands = self.band_count(bands)
        parameters = np.asarray(parameters, dtype=np.float64)
        if parameters.shape[-1:] != (3,) or not np.all(np.isfinite(parameters)):
            raise ValueError("finite three-dimensional parameters required")
        if np.any(np.linalg.norm(parameters, axis=-1) <= 0):
            raise ValueError("nonzero radial parameters required")
        unit = parameters / np.linalg.norm(parameters, axis=-1)[..., None]
        frequencies, amplitudes = (
            np.asarray(self.frequencies[:bands]),
            np.asarray(self.amplitudes[:bands]),
        )
        angles = (unit @ self.directions[:bands].T) * frequencies + self.phases[:bands]
        radius = 1 - sum(self.amplitudes) + np.sin(angles) @ amplitudes
        gradient = (np.cos(angles) * (amplitudes * frequencies)) @ self.directions[
            :bands
        ]
        gradient -= unit * np.sum(gradient * unit, axis=-1)[..., None]
        normals = (unit - gradient / radius[..., None]) / self.radii
        normals /= np.linalg.norm(normals, axis=-1)[..., None]
        return self.centre + unit * radius[..., None] * self.radii, normals

    def mesh(self, level, bands=None):
        parameters, faces = cube_mesh(level)
        vertices, normals = self.evaluate(parameters, bands)
        return {
            "parameters": parameters,
            "vertices": vertices,
            "faces": faces,
            "normals": normals,
        }

    def interpolation_bound(self, level, bands=None):
        bands = self.band_count(bands)
        if not isinstance(level, (int, np.integer)) or not 0 <= level <= 7:
            raise ValueError(
                "integer subdivision level between zero and seven required"
            )
        gradient = sum(
            a * f
            for a, f in zip(
                self.amplitudes[:bands], self.frequencies[:bands], strict=True
            )
        )
        hessian = sum(
            a * f * f
            for a, f in zip(
                self.amplitudes[:bands], self.frequencies[:bands], strict=True
            )
        )
        omitted = sum(self.amplitudes[bands:])
        curvature = max(self.radii) * (
            2 / np.sqrt(3) * (1 - omitted + gradient) + 2 * gradient + hessian
        )
        rounding = (
            np.finfo(np.float64).eps * (max(np.abs(self.centre)) + max(self.radii)) * 32
        )
        return curvature / (1 << (2 * level)) + max(self.radii) * omitted + rounding

    def projection_scale(self, camera):
        lower, upper = (
            np.asarray(self.centre) - self.radii,
            np.asarray(self.centre) + self.radii,
        )
        return camera.projected_span(lower, upper) / np.linalg.norm(upper - lower)

    def choose_level(self, camera, pixels=1.0, maximum=7):
        if (
            not np.isfinite(pixels)
            or pixels <= 0
            or not isinstance(maximum, (int, np.integer))
            or not 0 <= maximum <= 7
        ):
            raise ValueError(
                "positive finite pixel tolerance and bounded maximum level required"
            )
        scale = self.projection_scale(camera)
        for level in range(maximum + 1):
            error = self.interpolation_bound(level) * scale
            if error <= pixels:
                return level, error, True
        return maximum, error, False

    def choose_product(self, camera, pixels=1.0, maximum=7):
        baseline, error, satisfied = self.choose_level(camera, pixels, maximum)
        if not satisfied:
            return baseline, len(self.amplitudes), error, False
        scale = self.projection_scale(camera)
        for level in range(baseline + 1):
            for bands in range(len(self.amplitudes) + 1):
                error = self.interpolation_bound(level, bands) * scale
                if error <= pixels:
                    return level, bands, error, True
        raise AssertionError("full-band baseline must remain a feasible product")
