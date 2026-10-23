"""Current-view Ready queries and measured bounded pixel/transport integration."""

import argparse
import resource
import sys
import time
import tracemalloc

import numpy as np
from compare_urban_parents import opaque_image
from pixel_reference import integrate_fields
from triangle_image_reference import image_rays

STATES = (
    "resolved",
    "pending",
    "half_view",
    "half_caster",
    "half_ready",
    "all_ready",
    "foreground",
)


def accumulate_work(total, counts):
    for key, value in counts.items():
        if isinstance(value, dict):
            accumulate_work(total.setdefault(key, {}), value)
        elif key.endswith("peak_bytes") or key in {
            "parent_array_bytes",
            "background_ray_arrays_bytes",
        }:
            total[key] = max(total.get(key, 0), value)
        else:
            total[key] = total.get(key, 0) + value


def opaque_background(parent, origins, directions, profile, stop, **kwargs):
    """Zero unknown-parent contribution; keep mixed()'s actual opaque receiver."""
    return (
        np.zeros_like(origins),
        np.ones(len(origins)),
        np.ones(len(origins)),
        {"opaque_background_rays": len(origins)},
    )


def composed_scene(
    parent,
    resolved,
    profile,
    state,
    foreground_fraction,
    *,
    component="complete",
    receiver_ownership=False,
):
    if component not in {"complete", "correction", "background", "moments"}:
        raise ValueError("complete, correction, background or moments required")
    if state == "resolved":
        if component != "complete":
            raise ValueError("Ready state required for background/correction")
        return None
    from compare_urban_layers import layer_states, mixed

    matches = [value for value in layer_states(parent, resolved) if value[0] == state]
    if len(matches) != 1 or not 0 <= foreground_fraction <= 1:
        raise ValueError(
            "existing Ready state and bounded foreground fraction required"
        )
    _, child, caster, view_region, caster_region, foreground = matches[0]

    def query(origins, directions):
        options = {"correction_only": component == "correction"}
        if component == "background":
            options["layer_query"] = opaque_background
        if component == "moments":
            options["return_moments"] = True
        rgb, work = mixed(
            parent,
            origins,
            directions,
            profile,
            child,
            caster,
            view_region,
            caster_region,
            foreground,
            foreground_fraction=foreground_fraction,
            receiver_ownership=receiver_ownership,
            **options,
        )
        # Complete RGB includes the opaque ground/environment. This channel is
        # unused; it is not the resolved-only child-hit coverage diagnostic.
        return rgb, np.ones(len(origins)), work

    return query


def rss_bytes():
    scale = 1 if sys.platform == "darwin" else 1024
    return resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * scale


def integrate(resolved, view, box, profile, args, *, scene_query=None, channels=3):
    work = {}
    scene_seconds = 0.0

    def rays(pixel, samples, stream):
        low = np.asarray(pixel)
        return image_rays(
            view, np.array([low, low + 1]), 1, 1, samples, jitter_seed=stream
        )

    def scene(origins, directions):
        nonlocal scene_seconds
        started = time.perf_counter()
        rgb, alpha, counts = (
            opaque_image(resolved, origins, directions, profile)
            if scene_query is None
            else scene_query(origins, directions)
        )
        scene_seconds += time.perf_counter() - started
        accumulate_work(work, counts)
        return rgb, alpha

    if args.trace_memory:
        tracemalloc.start()
    started = time.perf_counter()
    rgb, alpha, pixels = integrate_fields(
        box,
        rays,
        scene,
        channels=channels,
        samples=args.samples,
        seed=args.seed,
        batch=args.batch,
    )
    seconds = time.perf_counter() - started
    peak = tracemalloc.get_traced_memory()[1] if args.trace_memory else None
    if args.trace_memory:
        tracemalloc.stop()
    return (
        rgb,
        alpha,
        {
            "integration_seconds": seconds,
            "scene_seconds": scene_seconds,
            "integration_traced_peak_B": peak,
            "process_peak_after_integration_B": rss_bytes(),
            "pixel_work": pixels,
            "scene_work": work,
        },
    )


def integrate_control(
    resolved, view, box, profile, args, model_query, background_query
):
    if not (
        type(args.samples) is int
        and type(args.background_samples) is int
        and 1 <= args.samples <= args.background_samples <= 128
        and args.state in STATES[1:]
    ):
        raise ValueError(
            "bounded background density at least correction density required"
        )
    phase_args = argparse.Namespace(**(vars(args) | {"trace_memory": False}))
    weighted = getattr(args, "weighted_transport", False)
    if args.trace_memory:
        tracemalloc.start()
    started = time.perf_counter()
    peak = None
    try:
        if args.state == "all_ready":
            width, height = np.asarray(box)[1] - np.asarray(box)[0]
            delta = None if weighted else np.zeros((height, width, 3))
            correction_work = {
                "integration_seconds": 0.0,
                "scene_seconds": 0.0,
                "pixel_work": {"rays": 0},
                "explicit_full_ready_zero_correction_pixels": int(width * height),
            }
        else:
            delta, _, correction_work = integrate(
                resolved,
                view,
                box,
                profile,
                phase_args,
                scene_query=model_query,
                channels=10 if weighted else 3,
            )
        phase_args.samples = args.background_samples
        background, _, background_work = integrate(
            resolved, view, box, profile, phase_args, scene_query=background_query
        )
        if weighted:
            from urban_transport_moments import compose_moments

            if delta is None:
                rgb, transport, fallback = background, np.ones_like(background), 0
                arrays = {"background": background, "weighted_transport": transport}
            else:
                rgb, transport, fallback = compose_moments(delta, background)
                arrays = {
                    "background": background,
                    "moments": delta,
                    "weighted_transport": transport,
                }
        else:
            rgb = background + delta
            arrays = {"background": background, "correction": delta}
    finally:
        if args.trace_memory:
            peak = tracemalloc.get_traced_memory()[1]
            tracemalloc.stop()
    seconds = time.perf_counter() - started
    if not np.isfinite(rgb).all():
        raise ValueError("finite composed control image required")
    return (
        rgb,
        arrays,
        {
            "integration_seconds": seconds,
            "scene_seconds": correction_work["scene_seconds"]
            + background_work["scene_seconds"],
            "integration_traced_peak_B": peak,
            "process_peak_after_integration_B": rss_bytes(),
            "moment_phase" if weighted else "correction_phase": correction_work,
            "background_phase": background_work,
            "method": (
                "background-weighted joint transport"
                if weighted
                else "dense background plus signed coarse correction"
            ),
            **({"fallback_transport_channels": fallback} if weighted else {}),
        },
    )
