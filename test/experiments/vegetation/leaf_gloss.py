"""Existing single-scatter GGX in an explicit area mixture with the sheet baseline."""

from dataclasses import dataclass

import numpy as np
from outshine_ref.vegetation.leaf_lighting import leaf_diffuse_response, unit_directions
from relief_shading_reference import specular_from_dots


def specular_response(normals, roughness, views, lights):
    normals = np.asarray(normals, float)
    if normals.ndim != 2 or normals.shape[1:] != (3,) or len(normals) > 1_048_576:
        raise ValueError("bounded normal rows required")
    normals, views, lights = (
        unit_directions(v, len(normals)) for v in (normals, views, lights)
    )
    if not np.isfinite(roughness) or not 0.05 <= roughness <= 1:
        raise ValueError("bounded perceptual roughness required; no delta lobe")
    facing = np.where(np.sum(normals * views, axis=1) < 0, -1, 1)
    normal = normals * facing[:, None]
    nl, nv = np.sum(normal * lights, axis=1), np.sum(normal * views, axis=1)
    active = (nl > 0) & (nv > 0)
    result = np.zeros(len(normals))
    if not active.any():
        return result
    half = views[active] + lights[active]
    half /= np.linalg.norm(half, axis=1)[:, None]
    nh = np.maximum(np.sum(normal[active] * half, axis=1), 0)
    vh = np.maximum(np.sum(views[active] * half, axis=1), 0)
    result[active] = specular_from_dots(
        roughness, nl[active], np.maximum(nv[active], 1e-6), nh, vh
    )
    return result


@dataclass(frozen=True)
class LeafGloss:
    roughness: float = 0.3
    mr_fraction: float = 0.12
    mode: str = "area"

    def __post_init__(self):
        if (
            not np.isfinite([self.roughness, self.mr_fraction]).all()
            or not 0.05 <= self.roughness <= 1
            or not 0 <= self.mr_fraction <= 1
            or self.mode not in ("area", "additive")
        ):
            raise ValueError(
                "bounded explicit single-scatter area-mixture experiment required"
            )

    def __call__(self, optical, normals, views, lights):
        sheet = leaf_diffuse_response(optical, normals, views, lights)
        if self.mr_fraction == 0:
            return sheet
        specular = specular_response(normals, self.roughness, views, lights)[:, None]
        keep = 1 - self.mr_fraction if self.mode == "area" else 1
        return keep * sheet + self.mr_fraction * specular

    def recipe(self):
        return {
            "model": "leaf-gloss-area-pilot-1",
            "mode": self.mode,
            "perceptual_roughness": self.roughness,
            "mr_area_fraction": self.mr_fraction,
            "dielectric_f0": 0.04,
            "f90": 1.0,
            "mr_base_colour": [0, 0, 0],
            "scope": "single-scatter correlated-Smith GGX from existing material reference, black absorbing MR part plus unchanged thin-sheet component; author parameters, not calibrated wax-layer or preserved whole-leaf hemispherical R/T",
        }
