"""Lossless, unpolarized thin-sheet model used by the scoped glass experiment.

Average the interface polarizations before summing internal reflections, matching
Mitsuba's unpolarized ThinDielectric. This is not a polarization-resolved slab.
The outgoing transmission direction is unchanged; absorption and interference
are absent. A finite pane's side exits are outside this infinite-sheet model.
"""

import math


def interface_reflectance(cosine, ior=1.52):
    """Air-to-dielectric Fresnel power, with outward incidence cosine in [0,1]."""
    if not math.isfinite(cosine) or not 0 <= cosine <= 1:
        raise ValueError("Outward incidence cosine must be finite and in [0,1]")
    if not math.isfinite(ior) or not 1 <= ior <= 3:
        raise ValueError("This experiment covers finite dielectric IOR in [1,3]")
    if ior == 1:
        return 0.0
    transmitted = math.sqrt(ior * ior - 1 + cosine * cosine)
    rs = (cosine - transmitted) / (cosine + transmitted)
    rp = (ior * ior * cosine - transmitted) / (ior * ior * cosine + transmitted)
    return (rs * rs + rp * rp) / 2


def sheet_coefficients(cosine, ior=1.52):
    """Return total reflected/transmitted power after the internal ray series."""
    fresnel = interface_reflectance(cosine, ior)
    return 2 * fresnel / (1 + fresnel), (1 - fresnel) / (1 + fresnel)


def lateral_displacement(cosine, thickness_m, ior=1.52):
    """Parallel-slab offset perpendicular to the outgoing ray, not on its plane.

    Bound for an infinite parallel slab, not an image-error guarantee. It ignores
    edge exits, occlusion changes and screen-space magnification near the camera.
    """
    interface_reflectance(cosine, ior)
    if not math.isfinite(thickness_m) or thickness_m < 0:
        raise ValueError("Thickness must be finite and nonnegative")
    if ior == 1:
        return 0.0
    sine = math.sqrt(1 - cosine * cosine)
    tangent_inside = sine / math.sqrt(ior * ior - sine * sine)
    return thickness_m * (sine - cosine * tangent_inside)
