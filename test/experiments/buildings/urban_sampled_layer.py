"""Sample urban radiance while retaining deterministic transport to opaque stops."""

import numpy as np
from urban_first_hit import merge_work
from urban_parent_conditional import covered_grid, ground_stops, transport_inputs
from urban_prefix_runs import PrefixRuns
from urban_sampled_parent import receiver_visibility, sampled_surfaces


def render_sampled_layer(
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
    samples=4,
    seed=1113,
    batch=2048,
    shadow_query=None,
):
    """Return parent C, miss T and miss-conditioned receiver-shadow S.

    Unit pixel rays use open finite stops; infinity means sky. The caller
    declares complete camera/caster ready cells, including vacancies, separately.
    Deterministic external visibility affects direct parent light; the caller
    already includes non-parent shadows in its opaque direct term. Compose
    C+T*(S*direct+other) jointly before filtering; signed corrections stay signed.
    """
    if (
        type(samples) is not int
        or not 1 <= samples <= 512
        or type(batch) is not int
        or not 1 <= batch <= 2048
    ):
        raise ValueError("bounded sample count and ray packet required")
    origins, directions, product, stop, receiver_normals = transport_inputs(
        parent,
        origins,
        directions,
        profile,
        stop,
        receiver_normals,
        primary_covered,
        caster_covered,
    )
    if not np.allclose(np.linalg.norm(directions, axis=1), 1, rtol=0, atol=1e-12):
        raise ValueError("unit view directions required for sampled radiance")
    stop = ground_stops(origins, directions) if stop is None else stop
    colour = np.zeros_like(origins)
    transmission, visibility = np.ones(len(origins)), np.ones(len(origins))
    work = {"surface_samples_per_ray": samples, "packets": 0}
    if covered_grid(product, product["primary_covered"]) and covered_grid(
        product, product["caster_covered"]
    ):
        work["full_ready_bypass_rays"] = len(origins)
        return colour, transmission, visibility, work
    model = PrefixRuns(parent)
    model.product = product
    generator = np.random.default_rng(np.random.SeedSequence(seed, spawn_key=(1,)))
    for first in range(0, len(origins), batch):
        part = slice(first, first + batch)
        o, d, stops = origins[part], directions[part], stop[part]
        t, prefix = model.transmission(o, d, stops)
        probability = np.maximum(1 - t, 0)
        s, shadow = receiver_visibility(
            model,
            o,
            d,
            stops,
            t,
            profile,
            shadow_query,
            None if receiver_normals is None else receiver_normals[part],
        )
        uniform = (
            np.arange(samples)[None] + generator.random((len(o), samples))
        ) / samples
        c, counts = sampled_surfaces(
            model,
            o,
            d,
            stops,
            probability,
            uniform,
            profile,
            shadow_query,
            group_first_hits=samples >= 4,
            external_visibility=external_visibility,
        )
        colour[part], transmission[part], visibility[part] = c, t, s
        work["packets"] += 1
        merge_work(work, prefix, "prefix_")
        merge_work(work, shadow, "receiver_shadow_")
        merge_work(work, counts)
    return colour, transmission, visibility, work
