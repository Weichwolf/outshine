"""Exactly one diffuse neighbour exchange in a declared finite opaque geometry."""

import numpy as np
from diffuse_scatter import sample_sheet
from outshine_ref.vegetation.leaf_lighting import leaf_diffuse_response


def surface_optics(ids, leaf_faces, state):
    leaf = np.asarray(state["leaf_rt"], float)
    ground = np.r_[state["ground_albedo"], np.zeros(3)]
    return np.where((ids < leaf_faces)[:, None], leaf, ground)


def direct_states(scene, points, normals, outgoing, optical, states, bias):
    answers = {name: np.zeros_like(points) for name in states}
    evaluated = {}
    for name, state in states.items():
        for lamp in state["lights"]:
            direction, energy = (
                np.asarray(lamp["direction"], float),
                np.asarray(lamp["irradiance_rgb"], float),
            )
            if (
                energy.shape != (3,)
                or not np.isfinite(energy).all()
                or np.any(energy < 0)
            ):
                raise ValueError("finite nonnegative beam RGB required")
            key = (*direction, *energy)
            if key not in evaluated:
                visible, work = scene.index.visibility(points, normals, direction, bias)
                for k, value in work.items():
                    scene.shadow_work[k] = scene.shadow_work.get(k, 0) + value
                evaluated[key] = (
                    leaf_diffuse_response(optical, normals, outgoing, direction)
                    * energy
                    * visible[:, None]
                )
            answers[name] += evaluated[key]
    return answers


def neighbour_sources(scene, points, normals, outgoing, optical, random, bias):
    if not np.isfinite(bias) or not 1e-7 <= bias <= 1e-3:
        raise ValueError("explicit bounded metric scattering bias required")
    directions, weights = sample_sheet(optical, normals, outgoing, random)
    active = np.flatnonzero(np.max(weights, axis=1) > 0)
    side = np.where(np.sum(normals[active] * directions[active], axis=1) >= 0, 1, -1)
    origins = points[active] + bias * side[:, None] * normals[active]
    ids, hit_points, hit_normals, _ = scene.nearest(
        origins, directions[active], scene.index.scale + 1
    )
    hit = ids >= 0
    return (
        active[hit],
        ids[hit],
        hit_points[hit],
        hit_normals[hit],
        -directions[active[hit]],
        weights[active[hit]],
    )


def neighbour_states(
    scene, points, normals, outgoing, optical, random, states, leaf_faces, bias
):
    materials = [
        (tuple(s["leaf_rt"]), tuple(s["ground_albedo"])) for s in states.values()
    ]
    if not materials or any(m != materials[0] for m in materials):
        raise ValueError(
            "shared optical materials required for joint light-state queries"
        )
    answers = {name: np.zeros_like(points) for name in states}
    selected, ids, source_points, source_normals, source_outgoing, weights = (
        neighbour_sources(scene, points, normals, outgoing, optical, random, bias)
    )
    if not len(selected):
        return answers
    source_optical = surface_optics(ids, leaf_faces, next(iter(states.values())))
    direct = direct_states(
        scene,
        source_points,
        source_normals,
        source_outgoing,
        source_optical,
        states,
        bias,
    )
    for name in states:
        answers[name][selected] = weights * direct[name]
    return answers
