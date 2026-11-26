"""Find the directional products used along a straight sweep of the eye."""

from time import perf_counter

import numpy as np


def needed_views(directions, eye_start, eye_end):
    scores = directions @ np.stack((eye_start, eye_end)).T
    left, right = np.triu_indices(len(directions), 1)
    begin = scores[left, 0] - scores[right, 0]
    finish = scores[left, 1] - scores[right, 1]
    crossing = (begin < 0) != (finish < 0)
    times = np.unique(np.concatenate(([0., 1.], begin[crossing] /
                                       (begin[crossing] - finish[crossing]))))
    probes = np.concatenate((times, (times[:-1] + times[1:]) * .5))
    rank = (1. - probes[:, None]) * scores[:, 0] + probes[:, None] * scores[:, 1]
    chosen = np.argsort(-rank, axis=1, kind="stable")[:, :2]
    return np.unique(chosen)


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
