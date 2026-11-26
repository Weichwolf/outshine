"""Find the directional products used along a straight sweep of the eye."""

from time import perf_counter

import numpy as np


def needed_views(directions, eye_start, eye_end, require_elevated=False):
    scores = directions @ np.stack((eye_start, eye_end)).T
    left, right = np.triu_indices(len(directions), 1)
    begin = scores[left, 0] - scores[right, 0]
    finish = scores[left, 1] - scores[right, 1]
    crossing = (begin < 0) != (finish < 0)
    times = np.unique(np.concatenate(([0., 1.], begin[crossing] /
                                       (begin[crossing] - finish[crossing]))))
    probes = np.concatenate((times, (times[:-1] + times[1:]) * .5))
    rank = (1. - probes[:, None]) * scores[:, 0] + probes[:, None] * scores[:, 1]
    if require_elevated:
        elevated = directions[:, 1] > 0
        if not np.any(elevated):
            return np.array([], dtype=np.int64)
        level = rank[:, ~elevated]
        if level.shape[1] >= 2:
            runner_up = np.sort(level, axis=1)[:, -2]
            margin = 1e-9 * max(np.linalg.norm(eye_start), np.linalg.norm(eye_end))
            if np.any(rank[:, elevated].max(axis=1) <= runner_up + margin):
                return np.array([], dtype=np.int64)
    chosen = np.argsort(-rank, axis=1, kind="stable")[:, :2]
    return np.unique(chosen)


def projection_ramp(half_extent, texels, focal, maximum_texel_px=1.):
    bound = np.sqrt(3.) * half_extent
    clear_distance = 2. * half_extent * focal / (texels * maximum_texel_px)
    return bound + clear_distance, bound + clear_distance / .985


def impostor_weight(distance, ramp):
    a, b = ramp
    weight = np.clip((distance - a) / (b - a), 0., 1.)
    return weight * weight * (3. - 2. * weight)


def validate_handoff():
    ramp = projection_ramp(94., 128, 880.)
    delta = ramp[1] - ramp[0]
    retreat = np.linspace(ramp[0] - delta, ramp[1] + delta, 120)
    weights = impostor_weight(retreat, ramp)
    np.testing.assert_allclose(impostor_weight(retreat[::-1], ramp)[::-1], weights)
    assert weights[0] == 0. and weights[-1] == 1.
    assert np.all(np.diff(weights) >= 0.)
    grid = np.indices((256, 256), dtype=np.float32)
    noise = np.mod(np.float32(52.9829189) * np.mod(
        grid[1] * np.float32(.06711056) + grid[0] * np.float32(.00583715), 1.), 1.)
    for target in (.1, .25, .5, .75, .9):
        group = noise < target
        mesh = noise >= target
        assert np.count_nonzero(group ^ mesh) == noise.size
        assert abs(np.mean(group) - target) < .005
    print(f"complementary masks through a {delta:.1f} m handoff band")


def validate():
    ring = np.linspace(0., 2 * np.pi, 12, endpoint=False)
    directions = np.stack((np.cos(ring), np.zeros(len(ring)), np.sin(ring)), axis=1)
    directions = np.vstack((directions, np.eye(3)[1]))
    stop = np.array([0.1, .2, 1.])
    assert len(needed_views(directions, stop, stop)) == 2
    west, east = np.array([-1., .4, -.4]), np.array([1., .4, -.3])
    start = set(needed_views(directions, west, west))
    finish = set(needed_views(directions, east, east))
    assert set(needed_views(directions, west, east)) > start | finish
    high_east = np.array([1., 2., 0.])
    high_west = np.array([-1., 2., 0.])
    assert len(needed_views(directions, high_east, high_west, require_elevated=True)) >= 2
    assert not len(needed_views(directions[:-1], high_east, high_west, require_elevated=True))
    high = np.array([0., .9, 1.])
    low = np.array([0., .1, 1.])
    assert len(needed_views(directions, high, high, require_elevated=True)) == 2
    assert not len(needed_views(directions, high, low, require_elevated=True))
    assert not len(needed_views(directions, low, high, require_elevated=True))

    rng = np.random.default_rng(1975)
    times = np.linspace(0, 1, 4097)[:, None]
    began = perf_counter()
    for west, east in rng.normal(size=(500, 2, 3)):
        poses = (1. - times) * west + times * east
        dense = np.argsort(-(poses @ directions.T), axis=1, kind="stable")[:, :2]
        exact = needed_views(directions, west, east)
        assert set(np.unique(dense)) <= set(exact)
    print(f"500 sweeps checked against 4097 samples each in {perf_counter() - began:.3f} s")


if __name__ == "__main__":
    validate()
    validate_handoff()
