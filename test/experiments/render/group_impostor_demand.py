"""Find the directional products used along a straight sweep of the eye."""

from time import perf_counter

import numpy as np

CANOPY_SCORE_WIDTH = .025


def canopy_weights(directions, eye):
    scores = directions @ eye / np.linalg.norm(eye)
    best = np.sort(scores)
    lower, upper = best[-2] - CANOPY_SCORE_WIDTH, best[-2]
    if len(best) == 2 or best[-3] <= lower:
        return (scores >= best[-2]).astype(float)
    for _ in range(14):
        cut = (lower + upper) * .5
        weights = np.clip((scores - cut) / CANOPY_SCORE_WIDTH, 0, 1)
        if weights.sum() > 2:
            lower = cut
        else:
            upper = cut
    return np.clip((scores - (lower + upper) * .5) / CANOPY_SCORE_WIDTH, 0, 1)


def canopy_demand(directions, start, finish):
    span = (CANOPY_SCORE_WIDTH + 1e-9) * max(np.linalg.norm(start), np.linalg.norm(finish))
    scores = directions @ np.stack((start, finish)).T
    chosen = []
    for index, candidate in enumerate(scores):
        events = []
        ahead = 0
        for begin, end in scores - candidate:
            ahead += begin > span
            if (begin > span) != (end > span):
                events.append(((span - begin) / (end - begin), 1 if end > span else -1))
        if ahead < 2:
            chosen.append(index)
            continue
        for _, delta in sorted(events):
            ahead += delta
            if ahead < 2:
                chosen.append(index)
                break
    return chosen


def validate_canopy_demand(directions):
    rng = np.random.default_rng(1975)
    t = np.linspace(0., 1., 4097)[:, None]
    began = perf_counter()
    for start, finish in rng.normal(size=(500, 2, 3)):
        eyes = (1 - t) * start + t * finish
        scores = (eyes / np.linalg.norm(eyes, axis=1)[:, None]) @ directions.T
        second = np.partition(scores, -2, axis=1)[:, -2]
        used = np.flatnonzero(np.any(second[:, None] - scores < CANOPY_SCORE_WIDTH, axis=0))
        selected = canopy_demand(directions, start, finish)
        assert set(used) <= set(selected)
        still = canopy_demand(directions, start, start)
        assert set(np.flatnonzero(canopy_weights(directions, start))) <= set(still)

    tied = np.array([[.6, .8, 0.], [-.6, .8, 0.], [0., .8, .6]])
    a = canopy_weights(tied, np.array([-1e-6, 1., 0.]))
    b = canopy_weights(tied, np.array([1e-6, 1., 0.]))
    np.testing.assert_allclose(a.sum(), 2., atol=.0002)
    np.testing.assert_allclose(a, b, atol=.0001)
    phases = (np.arange(4096) + .5) / 4096
    for weights in [a, b, np.array([1., .2, .8]), np.array([0., 1., 1.])]:
        coverage = np.zeros_like(phases)
        prefix = 0.
        for weight in weights:
            coverage += np.mod(phases - prefix, 1.) < weight
            prefix += weight
        assert np.mean(coverage == 2) > .999
    print(f"500 canopy sweeps conservatively cover 4097 poses each in {perf_counter()-began:.3f} s")


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


def validate_sample_coverage():
    y, x = np.indices((256, 256), dtype=np.int32)
    pixel = np.stack((x, y)).astype(np.float32)
    phase = np.mod(np.float32(52.9829189) * np.mod(
        pixel[0] * np.float32(.06711056) + pixel[1] * np.float32(.00583715), 1.), 1.)
    rotation = (x + 2 * y) & 3

    def mask(begin, end):
        first = np.clip(np.ceil(4 * begin - phase), 0, 4).astype(np.int32)
        past = np.clip(np.ceil(4 * end - phase), 0, 4).astype(np.int32)
        bits = (1 << past) - (1 << first)
        return ((bits << rotation) | (bits >> (4 - rotation))) & 15

    for coverage in (.1, .25, .37, .5, .75, .9):
        group, mesh = mask(0, coverage), mask(coverage, 1)
        assert np.all((group & mesh) == 0)
        assert np.all((group | mesh) == 15)
        share = np.mean((group[..., None] & (1 << np.arange(4))) != 0, axis=-1)
        assert abs(share.mean() - coverage) < .005
        assert np.max(np.abs(share - coverage)) < .25
        single_mse = np.mean(((phase < coverage) - coverage) ** 2)
        multisample_mse = np.mean((share - coverage) ** 2)
        assert multisample_mse < single_mse / 4
        if coverage == .37:
            print(f"37% pixel coverage MSE: {single_mse:.4f} -> {multisample_mse:.4f}")


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
    validate_sample_coverage()
    import argparse
    import json
    from pathlib import Path

    parser = argparse.ArgumentParser()
    parser.add_argument("--layout", type=Path)
    args = parser.parse_args()
    if args.layout:
        views = np.array(json.loads(args.layout.read_text())["directions"])
    else:
        angles = np.arange(8) * np.pi / 4
        views = np.column_stack((np.sin(angles), np.zeros_like(angles), np.cos(angles)))
        views = np.vstack((views, [0, 1, 0]))
    validate_canopy_demand(views)
