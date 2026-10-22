"""Urban-only premultiplied radiance and transport to a caller's opaque stop."""

import numpy as np
from urban_parent_conditional import trace_transport
from urban_parent_light import shade


def render_layer(
    parent,
    origins,
    directions,
    profile,
    stop,
    *,
    receiver_normals=None,
    primary_covered=None,
    caster_covered=None,
    external_visibility=None,
    batch=2048,
    transmission_tolerance=0.0,
):
    """Coverage rectangles mean complete ready cells, including known vacancies.

    Stops use the same ray parameter as origins/directions; infinity means sky.
    Camera and caster replacement are independent caller declarations. The parent
    product alone never supplies either declaration. External visibility, when
    present, is deterministic visibility of additional opaque casters.
    """
    origins, directions = np.asarray(origins, float), np.asarray(directions, float)
    colour = np.zeros_like(origins)
    receiver_visibility = np.ones(len(origins))

    def receive(rays, slots, points, normals, weight, visibility):
        receiver = slots == len(parent.heights) * 5
        receiver_visibility[rays[receiver]] = visibility[receiver]
        at = rays[~receiver]
        points, normals = points[~receiver], normals[~receiver]
        visibility = visibility[~receiver]
        if external_visibility is not None:
            external = np.asarray(external_visibility(points, normals), float)
            if (
                external.shape != visibility.shape
                or not np.isfinite(external).all()
                or np.any((external < 0) | (external > 1))
            ):
                raise ValueError("external caster visibility must be finite in [0, 1]")
            visibility = visibility * external
        colour[at] += weight[~receiver, None] * shade(
            points, normals, -directions[at], 0.6, visibility, profile
        )

    coverage, work = trace_transport(
        parent,
        origins,
        directions,
        profile,
        receive,
        batch=batch,
        stop=stop,
        receiver_normals=receiver_normals,
        primary_covered=primary_covered,
        caster_covered=caster_covered,
        transmission_tolerance=transmission_tolerance,
    )
    return colour, 1 - coverage, receiver_visibility, work


def compose(colour, transmission, receiver_visibility, direct, other):
    """Background direct already includes its non-parent shadow visibility."""
    return colour + transmission[:, None] * (
        receiver_visibility[:, None] * direct + other
    )


def correction(colour, transmission, receiver_visibility, direct, other):
    """Signed difference from the same opaque scene without the parent.

    Add its filtered value to an independently filtered opaque baseline. With
    complete replacement it is exactly zero, preserving the baseline's sampling.
    """
    return (
        colour
        + (transmission * receiver_visibility - 1)[:, None] * direct
        + (transmission - 1)[:, None] * other
    )
