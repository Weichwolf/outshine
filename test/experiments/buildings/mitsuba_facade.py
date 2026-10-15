"""Unlit analytic facade coverage in the physical CPU reference renderer."""

import drjit as dr
import mitsuba as mi
import numpy as np


def validate(mesh, specs):
    values = np.asarray(mesh.get("facade_extent", []))
    if values.shape != (len(mesh["vertices"]), 3) or not np.isfinite(values).all():
        raise ValueError("Analytic facade shader requires its declared extent channel")
    selected = np.isin(mesh["materials"], [int(key) for key in specs])
    corners = values[mesh["faces"][selected]]
    if len(corners) and (
        np.any(corners[:, :, :2] < 1)
        or np.any(corners[:, :, :2] != np.floor(corners[:, :, :2]))
        or not np.array_equal(corners, np.repeat(corners[:, :1], 3, axis=1))
    ):
        raise ValueError(
            "Analytic facade shader requires one positive repeat domain per actual face"
        )
    for spec in specs.values():
        fractions = np.asarray(spec["fractions"])
        if (
            fractions.shape != (2,)
            or not np.isfinite(fractions).all()
            or np.any((fractions < 0) | (fractions > 1))
        ):
            raise ValueError(
                "Analytic facade shader fractions must lie in the unit interval"
            )


def bind():
    class Coverage(mi.Texture):
        def __init__(self, props):
            super().__init__(props)
            self.fractions = [
                float(props.get("width", 0.48)),
                float(props.get("height", 0.53)),
            ]
            self.eye = mi.Point3f(props["eye"])
            self.right = mi.Vector3f(props["right"])
            self.up = mi.Vector3f(props["up"])
            self.forward = mi.Vector3f(props["forward"])
            self.focal = float(props["focal_px"])
            self.filtered = bool(props.get("filtered", True))

        def footprint(self, si):
            delta = si.p - self.eye
            depth = dr.dot(delta, self.forward)
            ray = delta / dr.maximum(depth, 1e-6)
            denominator = dr.dot(si.n, ray)
            safe = dr.select(
                dr.abs(denominator) > 1e-7, denominator, dr.copysign(1e-7, denominator)
            )
            dx = (
                depth
                / self.focal
                * (self.right - ray * (dr.dot(si.n, self.right) / safe))
            )
            dy = depth / self.focal * (self.up - ray * (dr.dot(si.n, self.up) / safe))
            a = si.dp_du
            b = si.dp_dv
            aa = dr.dot(a, a)
            ab = dr.dot(a, b)
            bb = dr.dot(b, b)
            det = aa * bb - ab * ab
            inverse = 1 / dr.maximum(det, 1e-20)

            def projected(value):
                return mi.Vector2f(
                    (dr.dot(value, a) * bb - dr.dot(value, b) * ab) * inverse,
                    (dr.dot(value, b) * aa - dr.dot(value, a) * ab) * inverse,
                )

            x, y = projected(dx), projected(dy)
            return dr.maximum(dr.abs(x) + dr.abs(y), mi.Vector2f(1e-5))

        def rectangle(self, si, footprint, extent, fractions):
            result = mi.Float(1.0)
            for axis, fraction in enumerate(fractions):
                low = si.uv[axis] - footprint[axis] / 2
                high = si.uv[axis] + footprint[axis] / 2

                def primitive(value, axis=axis, fraction=fraction):
                    clipped = dr.clip(value, 0.0, extent[axis])
                    whole = dr.floor(clipped)
                    return whole * fraction + dr.clip(
                        clipped - whole - (1 - fraction) / 2, 0.0, fraction
                    )

                cell = dr.floor(si.uv[axis])
                contained = (
                    (low >= cell + (1 - fraction) / 2)
                    & (high <= cell + (1 + fraction) / 2)
                    & (cell >= 0)
                    & (cell < extent[axis])
                )
                result *= dr.select(
                    contained, 1.0, (primitive(high) - primitive(low)) / footprint[axis]
                )
            return dr.clip(result, 0.0, 1.0)

        def eval_1(self, si, active=True):
            extent = si.shape.eval_attribute_3("vertex_facade_extent", si, active)
            footprint = self.footprint(si) if self.filtered else mi.Vector2f(1e-5)
            return self.rectangle(si, footprint, extent, self.fractions)

        def eval(self, si, active=True):
            return mi.Spectrum(self.eval_1(si, active))

        def eval_3(self, si, active=True):
            return mi.Color3f(self.eval_1(si, active))

        def is_spatially_varying(self):
            return True

        def mean(self):
            return self.fractions[0] * self.fractions[1]

        def to_string(self):
            return "CameraBoxFacadeCoverage[]"

    mi.register_texture("outshine_facade_coverage", lambda props: Coverage(props))
    from mitsuba_facade_finish import register

    register(Coverage)


def material(spec, config, wall):
    from demand import View

    view = View(
        config["eye"],
        config["target"],
        width=config["profile"][0],
        height=config["profile"][1],
        fov_deg=config["fov_deg"],
    )
    texture = {
        "type": "outshine_facade_coverage",
        "width": spec["fractions"][0],
        "height": spec["fractions"][1],
        "eye": mi.ScalarPoint3f(view.eye),
        "right": mi.ScalarVector3f(view.basis[0]),
        "up": mi.ScalarVector3f(view.basis[1]),
        "forward": mi.ScalarVector3f(view.basis[2]),
        "focal_px": view.focal_px,
        "filtered": spec.get("filtered", True),
    }
    glazing = {
        "type": "roughplastic",
        "distribution": "ggx",
        "alpha": 0.08,
        "int_ior": 1.52,
        "ext_ior": 1.0,
        "diffuse_reflectance": {"type": "rgb", "value": [0.025, 0.035, 0.04]},
    }
    if config.get("facade_finishes") and not np.array_equal(
        spec["fractions"], [1.0, 1.0]
    ):
        from mitsuba_facade_finish import material as finished

        return finished(texture, config["facade_finishes"], wall, glazing), None
    return {
        "type": "blendbsdf",
        "weight": texture,
        "bsdf_0": wall,
        "bsdf_1": glazing,
    }, None
