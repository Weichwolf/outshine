"""Physical CPU reference for filtered facade frames and opaque floor bands."""

from copy import deepcopy

import drjit as dr
import mitsuba as mi
import numpy as np
from outshine_ref.buildings.facade_finish import FacadeFinish


def axis_coverage(centre, footprint, fraction, count, boundary=False):
    origin = dr.floor(centre)
    middle = centre - origin
    low, high = middle - footprint / 2, middle + footprint / 2
    domain_low, domain_high = -origin, count - origin

    def primitive(value):
        whole = dr.floor(value)
        part = value - whole
        if boundary:
            return (
                whole * fraction
                + dr.clip(part, 0.0, fraction / 2)
                + dr.clip(part - 1 + fraction / 2, 0.0, fraction / 2)
            )
        return whole * fraction + dr.clip(part - (1 - fraction) / 2, 0.0, fraction)

    a, b = dr.clip(low, domain_low, domain_high), dr.clip(high, domain_low, domain_high)
    result = (primitive(b) - primitive(a)) / footprint
    midpoint = dr.floor(middle + 0.5) if boundary else 0.5
    contained = (low >= midpoint - fraction / 2) & (high <= midpoint + fraction / 2)
    if not boundary:
        contained |= fraction >= 1
    contained &= (low >= domain_low) & (high <= domain_high)
    return dr.select(contained, 1.0, dr.clip(result, 0.0, 1.0))


def register(coverage):
    class FinishCoverage(coverage):
        def __init__(self, props):
            super().__init__(props)
            self.frame = float(props.get("frame_m", 0.0))
            self.band = float(props.get("band_m", 0.0))
            self.mode = str(props["mode"])
            self.extent_fields = (
                [props["extent_" + axis] for axis in "xyz"]
                if "extent_x" in props
                else None
            )
            if self.mode not in ("opening", "glass_ratio", "band_ratio"):
                raise ValueError("Unknown facade finish coverage mode")

        def eval_1(self, si, active=True):
            extent = (
                mi.Vector3f(
                    *[
                        dr.gather(mi.Float, value, si.prim_index, active)
                        for value in self.extent_fields
                    ]
                )
                if self.extent_fields is not None
                else si.shape.eval_attribute_3("vertex_facade_extent", si, active)
            )
            footprint = self.footprint(si) if self.filtered else mi.Vector2f(1e-5)
            opening = self.rectangle(si, footprint, extent, self.fractions)
            if self.mode == "opening":
                return opening
            size = [
                dr.maximum(dr.norm(si.dp_du), 1e-6),
                dr.maximum(dr.norm(si.dp_dv), 1e-6),
            ]
            if self.mode == "glass_ratio":
                fractions = [
                    dr.maximum(f - 2 * self.frame / size[i], 0.0)
                    for i, f in enumerate(self.fractions)
                ]
                glass = dr.minimum(
                    opening, self.rectangle(si, footprint, extent, fractions)
                )
                return dr.clip(glass / dr.maximum(opening, 1e-12), 0.0, 1.0)
            x = axis_coverage(si.uv.x, footprint.x, 1.0, extent.x)
            fraction = dr.minimum(self.band / size[1], 1 - self.fractions[1])
            band = dr.clip(
                x * axis_coverage(si.uv.y, footprint.y, fraction, extent.y, True),
                0.0,
                1 - opening,
            )
            return dr.clip(band / dr.maximum(1 - opening, 1e-12), 0.0, 1.0)

        def rectangle(self, si, footprint, extent, fractions):
            return axis_coverage(
                si.uv.x, footprint.x, fractions[0], extent.x
            ) * axis_coverage(si.uv.y, footprint.y, fractions[1], extent.y)

        def mean(self):
            return 0.5

    mi.register_texture("outshine_facade_finish", lambda props: FinishCoverage(props))


def material(texture, parameters, wall, glazing):
    finish = FacadeFinish(
        parameters.get("frame_m", 0.06), parameters.get("band_m", 0.16)
    )
    tint = parameters.get("band_tint", 0.65)
    if not np.isfinite(tint) or not 0 <= tint <= 1:
        raise ValueError("Facade band tint must lie in [0,1]")
    band = deepcopy(wall)
    surface = band
    while surface.get("type") in ("twosided", "normalmap", "mask"):
        surface = surface["nested"]
    colour = surface.get("base_color", {})
    if surface.get("type") != "principled" or colour.get("type") != "rgb":
        raise NotImplementedError(
            "Facade band reference needs an explicitly bound RGB parent material"
        )
    colour["value"] = (np.asarray(colour["value"]) * tint).tolist()
    frame = {
        "type": "principled",
        "base_color": {"type": "rgb", "value": [0.12, 0.14, 0.16]},
        "metallic": 0.65,
        "roughness": 0.28,
    }

    def weight(mode):
        return texture | {
            "type": "outshine_facade_finish",
            "frame_m": finish.frame_m,
            "band_m": finish.band_m,
            "mode": mode,
        }

    opaque = {
        "type": "blendbsdf",
        "weight": weight("band_ratio"),
        "bsdf_0": wall,
        "bsdf_1": band,
    }
    opening = {
        "type": "blendbsdf",
        "weight": weight("glass_ratio"),
        "bsdf_0": frame,
        "bsdf_1": glazing,
    }
    return {
        "type": "blendbsdf",
        "weight": weight("opening"),
        "bsdf_0": opaque,
        "bsdf_1": opening,
    }
