"""Compose registered opaque point fields before the existing C/T prefix.

The producer owns whole-source coverage and conservative depth error. A matching
binding is necessary, not proof of either. Unknown samples retain the parent;
this reference supplies no novel-view, shadow or continuous-area authority.
"""

from dataclasses import dataclass

import numpy as np
from field_composition import opaque_points


@dataclass(frozen=True)
class SampleBinding:
    """Existing native keys; capture also binds exact raster/sample locations.

    The input manifest retains the actual camera, clips and sample pattern;
    capture is its canonical description or content hash, never a view name.
    sources lists complete immutable group products, not selected visible faces.
    """

    frame: str
    capture: str
    world_revision: str
    sources: tuple
    materials: str
    lighting: str
    pbr_view: str

    def __post_init__(self):
        strings = (
            self.frame,
            self.capture,
            self.world_revision,
            self.materials,
            self.lighting,
            self.pbr_view,
        )
        if (
            not all(isinstance(v, str) and v for v in strings)
            or not isinstance(self.sources, tuple)
            or not self.sources
            or any(not isinstance(v, str) or not v for v in self.sources)
            or len(set(self.sources)) != len(self.sources)
        ):
            raise ValueError(
                "Explicit whole-source, raster and radiance bindings required"
            )


@dataclass(frozen=True)
class PointSamples:
    binding: SampleBinding
    rgba: np.ndarray
    reverse_z: np.ndarray
    error: np.ndarray
    known: np.ndarray


@dataclass(frozen=True)
class Composition:
    reason: str
    rgba: object = None
    reverse_z: object = None
    error: object = None
    bindings: tuple = ()


def validate(samples, texels_most):
    opaque_points(samples.rgba, texels_most)
    shape = samples.rgba.shape[:2]
    z, e, known = samples.reverse_z, samples.error, samples.known
    if (
        not isinstance(samples.binding, SampleBinding)
        or any(not isinstance(a, np.ndarray) or a.shape != shape for a in (z, e, known))
        or z.dtype != samples.rgba.dtype
        or e.dtype not in (np.dtype(np.float32), np.dtype(np.float64))
        or known.dtype != np.bool_
        or not np.isfinite(z).all()
        or not np.isfinite(e).all()
        or np.any((z < 0) | (z > 1))
        or np.any(e < 0)
        or np.any((z > 0) != (samples.rgba[:, :, 3] == 1))
        or np.any(e[z == 0] != 0)
    ):
        raise ValueError(
            "Coupled finite opaque coverage, Reverse-Z/error and known mask required"
        )


def interval(samples):
    z, e = samples.reverse_z.astype(float), samples.error.astype(float)
    with np.errstate(over="ignore"):
        low = np.maximum(0, np.nextafter(z - e, -np.inf))
        high = np.minimum(1, np.nextafter(z + e, np.inf))
    return low, high


def compose_registered(a, b, *, wanted_a, wanted_b, texels_most=262144):
    """Return a complete point product, or keep the whole parent without output.

    Reverse-Z is larger for nearer hits. Error is an absolute conservative bound
    in that same [0,1] coordinate, not metres or an observed maximum. Clear black
    is a proven miss only where known is true. Equality/overlap retains the parent.
    Caller-owned inputs are borrowed for this synchronous operation and unchanged.
    """
    if type(texels_most) is not int or texels_most < 1:
        raise ValueError("Positive point capacity required")
    if a.binding != wanted_a or b.binding != wanted_b:
        return Composition("changed_source_material_or_demand")
    for name in ("frame", "capture", "world_revision", "lighting", "pbr_view"):
        if getattr(a.binding, name) != getattr(b.binding, name):
            return Composition("unregistered_rays_or_radiance")
    validate(a, texels_most)
    validate(b, texels_most)
    if a.rgba.shape != b.rgba.shape or a.rgba.dtype != b.rgba.dtype:
        raise ValueError(
            "Matching registered point shapes and arithmetic profiles required"
        )
    if not a.known.all() or not b.known.all():
        return Composition("incomplete_sample_domain")
    hit_a, hit_b = a.reverse_z > 0, b.reverse_z > 0
    low_a, high_a = interval(a)
    low_b, high_b = interval(b)
    common = hit_a & hit_b
    first_a, first_b = low_a > high_b, low_b > high_a
    if np.any(common & ~(first_a | first_b)):
        return Composition("ambiguous_depth_retains_parent")
    choose_a = hit_a & (~hit_b | first_a)
    return Composition(
        "complete_registered_points",
        np.where(choose_a[:, :, None], a.rgba, b.rgba),
        np.where(choose_a, a.reverse_z, b.reverse_z),
        np.where(choose_a, a.error, b.error),
        (a.binding, b.binding),
    )
