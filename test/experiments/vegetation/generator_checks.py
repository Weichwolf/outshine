"""Independent hierarchy counts, pose invariants, cache and request checks."""

from dataclasses import replace
from unittest.mock import patch

import numpy as np
from generator_examples import mixed_graph
from generator_graph import GeneratorGraph
from generator_specs import Generator, Placement, TubeShape


def check_requests():
    graph = mixed_graph()
    assert graph.counts == {0: 1, 1: 1, 2: 6, 3: 48, 4: 7}
    assert not graph._poses
    assert graph.instances(4, 14) is None and not graph._poses
    fronds = graph.instances(4, 15)
    assert len(fronds) == 7 and len(graph._poses) == 15
    assert all(a is b for a, b in zip(fronds, graph.instances(4, 0)))
    path = ((0, 0), (1, 0), (2, 3), (3, 5))
    partial = mixed_graph()
    one = partial.instance(path)
    assert len(partial._poses) == 4
    assert partial.instances(3, 51) is None and len(partial._poses) == 4
    all_leaves = partial.instances(3, 52)
    assert len(all_leaves) == 48 and one is next(
        p for p in all_leaves if p.path == path
    )
    restored = GeneratorGraph.from_recipe(graph.recipe())
    assert restored.identity == graph.identity
    reordered = GeneratorGraph(graph.seed, tuple(reversed(graph.nodes)))
    relabelled = GeneratorGraph(
        graph.seed, tuple(replace(n, label="Name " + str(n.id)) for n in graph.nodes)
    )
    assert graph.identity == reordered.identity == relabelled.identity
    for key in graph.order:
        first, second = graph.instances(key, 1000), reordered.instances(key, 1000)
        assert len(first) == len(second) == graph.counts[key]
        assert len({p.path for p in first}) == len(first)
        for a, b in zip(first, second):
            assert a.path == b.path and a.scale == b.scale
            np.testing.assert_array_equal(a.origin, b.origin)
            np.testing.assert_array_equal(a.frame, b.frame)
            np.testing.assert_allclose(a.frame.T @ a.frame, np.eye(3), atol=2e-14)
            assert np.linalg.det(a.frame) > 1 - 2e-14
            assert not a.origin.flags.writeable and not a.frame.flags.writeable
    return {
        "instances": sum(graph.counts.values()),
        "partial_poses": 4,
        "frond_poses": 15,
    }


def check_pitch_continuity():
    root = Generator(0, "root", "root", ())
    frames = []
    for pitch in (np.pi / 2 - 1e-7, np.pi / 2, np.pi / 2 + 1e-7):
        placement = Placement(
            pitch=pitch, pitch_variance=0, azimuth_variance=0, scale_variance=0
        )
        graph = GeneratorGraph(
            17, (root, Generator(1, "axis", "axis", (0,), TubeShape(), placement))
        )
        frames.append(graph.instance(((0, 0), (1, 0))).frame)
    assert np.max(abs(frames[0] - frames[2])) < 2.1e-7
    return float(np.max(abs(frames[0] - frames[2])))


def check_invalid():
    root = Generator(0, "root", "root", ())
    cases = (
        (root, root),
        (root, Generator(1, "missing", "axis", (3,), TubeShape())),
        (
            root,
            Generator(1, "cycle1", "axis", (2,), TubeShape()),
            Generator(2, "cycle2", "axis", (1,), TubeShape()),
        ),
    )
    for nodes in cases:
        try:
            GeneratorGraph(17, nodes)
        except ValueError:
            pass
        else:
            raise AssertionError("invalid graph accepted")
    graph = mixed_graph()
    for path in (((0, 0), (3, 0)), ((0, 0), (1, 0), (2, 6)), ((0, 0), (99, 0))):
        try:
            graph.instance(path)
        except ValueError:
            pass
        else:
            raise AssertionError("invalid hierarchy path accepted")
    assert not graph._poses
    empty = GeneratorGraph(
        17,
        (root, Generator(1, "disabled", "axis", (0,), TubeShape(), Placement(count=0))),
    )
    assert empty.bounds[1] == 0
    assert empty.instances(1, 0) == () and not empty._poses
    large = GeneratorGraph(
        17,
        (root,)
        + tuple(
            Generator(
                key, "large", "axis", (key - 1,), TubeShape(), Placement(count=64)
            )
            for key in range(1, 4)
        ),
    )
    assert large.counts[3] == 64**3
    assert large.instances(3, 1_000_000) is None and not large._poses
    assert large.instance(((0, 0), (1, 63), (2, 63), (3, 63))).path[-1] == (3, 63)
    assert len(large._poses) == 4
    capped = mixed_graph()
    with patch("generator_graph.MAX_POSES", 3):
        try:
            capped.instance(((0, 0), (1, 0), (2, 0), (3, 0)))
        except ValueError:
            pass
        else:
            raise AssertionError("direct instance ignored the pose cap")
    assert not capped._poses
    return {"invalid_graphs": len(cases), "invalid_paths": 3, "disabled_no_work": True}
