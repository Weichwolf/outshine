"""Sample first-hit surface radiance; keep coverage/background analytical.

The real-arithmetic estimator targets the existing categorical model. Finite
sampling, floating evaluation and temporal stability require separate evidence;
this is no persisted random world, native replacement or visibility certificate.
"""

import numpy as np
from urban_first_hit import sample_first_hits
from urban_parent_conditional import ground_stops, surface_slots, surface_visibility
from urban_parent_light import shade
from urban_prefix_runs import PrefixRuns, clipped_span


def merge_work(target, source, prefix=""):
    for key, value in source.items():
        if isinstance(value, dict):
            merge_work(target, value, prefix + key + "_")
        else:
            name = prefix + key
            target[name] = (
                max(target.get(name, 0), value)
                if key.endswith("peak_bytes")
                else target.get(name, 0) + value
            )


def shadow_work():
    return dict.fromkeys(
        [
            "steps",
            "cell_ray_visits",
            "primary_condition_tests",
            "conditioning_peak_bytes",
            "shared_wall_visibilities",
        ],
        0,
    )


def background_radiance(
    model, origins, directions, stops, transmission, profile, query
):
    product = model.product
    local = origins - product["reference_origin_m"]
    _, end, _ = clipped_span(product, local, directions, stops)
    rays = np.flatnonzero(np.isfinite(stops) & (transmission > 0))
    colour = np.zeros_like(origins)
    work = shadow_work()
    points = (
        local[rays]
        + stops[rays, None] * directions[rays]
        + product["reference_origin_m"]
    )
    normals = np.tile([0.0, 0.0, 1.0], (len(rays), 1))
    visible = surface_visibility(
        model.parent,
        product,
        points,
        normals,
        (local[rays], directions[rays], np.maximum(end[rays], 0.1)),
        np.full((len(rays), 2), -1),
        np.zeros(len(rays), int),
        np.arange(len(rays)),
        profile,
        work,
        None,
        shadow_query=query,
    )
    colour[rays] = transmission[rays, None] * shade(
        points, normals, -directions[rays], 0.2, visible, profile
    )
    return colour, work


def sampled_surfaces(
    model, origins, directions, stops, probability, uniform, profile, query
):
    count = uniform.shape[1]
    rays = np.flatnonzero(probability > 0)
    owners = np.repeat(rays, count)
    thresholds = (uniform[rays] * probability[rays, None]).ravel()
    thresholds = np.minimum(thresholds, np.nextafter(probability[owners], 0))
    hit, primary = sample_first_hits(
        model.parent,
        model.product,
        origins[owners],
        directions[owners],
        thresholds,
        stops[owners],
    )
    if not np.isfinite(hit["depth"]).all():
        raise ArithmeticError(
            "conditional hit probability failed to produce a first event"
        )
    keys = np.c_[owners, hit["cell"], hit["depth"], hit["normal"]]
    _, index, inverse = np.unique(keys, axis=0, return_index=True, return_inverse=True)
    unique_rays = owners[index]
    depth, normal, cell, level = (
        hit[k][index] for k in ["depth", "normal", "cell", "height_index"]
    )
    product = model.product
    local = origins[unique_rays] - product["reference_origin_m"]
    direction = directions[unique_rays]
    points = local + depth[:, None] * direction + product["reference_origin_m"]
    shadows = shadow_work()
    visible = surface_visibility(
        model.parent,
        product,
        points,
        normal,
        (local, direction, depth),
        cell,
        surface_slots(level, normal),
        np.arange(len(index)),
        profile,
        shadows,
        None,
        shadow_query=query,
    )
    radiance = shade(points, normal, -direction, 0.6, visible, profile)
    weights = (
        np.bincount(inverse, minlength=len(index)) / count * probability[unique_rays]
    )
    colour = np.zeros_like(origins)
    np.add.at(colour, unique_rays, weights[:, None] * radiance)
    work = {
        "surface_samples": len(owners),
        "unique_surface_queries": len(index),
        "surface_sample_arrays_peak_bytes": sum(
            v.nbytes
            for v in [
                owners,
                thresholds,
                keys,
                index,
                inverse,
                depth,
                normal,
                cell,
                level,
                points,
                radiance,
                weights,
            ]
        )
        + sum(v.nbytes for v in hit.values()),
    }
    merge_work(work, primary, "primary_")
    merge_work(work, shadows, "shadow_")
    return colour, work


def render_sampled(
    parent,
    origins,
    directions,
    profile,
    *,
    samples=1,
    seed=1113,
    batch=512,
    shadow_query=None,
):
    if (
        type(samples) is not int
        or not 1 <= samples <= 512
        or type(batch) is not int
        or not 1 <= batch <= 2048
    ):
        raise ValueError("bounded sample count and ray packet required")
    if profile["kind"] == "point" and profile["position"][2] <= parent.heights[-1]:
        raise ValueError("point emitter must be above the model")
    origins, directions = np.asarray(origins, float), np.asarray(directions, float)
    if origins.ndim != 2 or origins.shape[1] != 3 or directions.shape != origins.shape:
        raise ValueError("matching finite unit view rays required")
    if not np.allclose(np.linalg.norm(directions, axis=1), 1, rtol=0, atol=1e-12):
        raise ValueError("unit view directions required")
    model = PrefixRuns(parent)
    query = shadow_query
    generator = np.random.default_rng(np.random.SeedSequence(seed, spawn_key=(1,)))
    colour, coverage = np.zeros_like(origins), np.zeros(len(origins))
    work = {"surface_samples_per_ray": samples, "packets": 0}
    for first in range(0, len(origins), batch):
        part = slice(first, first + batch)
        o, d = origins[part], directions[part]
        stops = ground_stops(o, d)
        transmission, prefix = model.transmission(o, d, stops)
        probability = np.maximum(1 - transmission, 0)
        background, shadow = background_radiance(
            model, o, d, stops, transmission, profile, query
        )
        uniform = (
            np.arange(samples)[None] + generator.random((len(o), samples))
        ) / samples
        surface, counts = sampled_surfaces(
            model, o, d, stops, probability, uniform, profile, query
        )
        colour[part], coverage[part] = background + surface, probability
        work["packets"] += 1
        merge_work(work, prefix, "prefix_")
        merge_work(work, shadow, "background_shadow_")
        merge_work(work, counts)
    return colour, coverage, work
