"""Independent recurrence, groups, graph demands and adversarial site budgets."""

from dataclasses import replace
from unittest.mock import patch

import numpy as np
from blade_surface import Blade
from generator_graph import GeneratorGraph
from generator_specs import Generator, Placement, TubeShape
from placement_patterns import DistancePattern
from profile_curve import Profile


def scalar_tables(pattern, lengths, first, last):
    result = []
    knots = pattern.spacing.table
    for length in lengths:
        rows, t, group = [], last, 0
        while (
            length > 0
            and first < last
            and t >= first - 32 * np.finfo(float).eps
            and group < pattern.limit_groups
        ):
            t = max(t, first)
            for member in range(pattern.whorl_size):
                rows.append(
                    (
                        t,
                        group * pattern.divergence
                        + member * 2 * np.pi / pattern.whorl_size,
                    )
                )
            x = min(max(t, 0), 1) * (len(knots) - 1)
            index = min(int(x), len(knots) - 2)
            step = float(knots[index]) * (1 - (x - index)) + float(knots[index + 1]) * (
                x - index
            )
            t -= step / length
            group += 1
        result.append(np.array(rows, dtype=float).reshape(-1, 2))
    return tuple(result)


def pattern_cases():
    return (
        DistancePattern(divergence=np.pi),
        DistancePattern(whorl_size=2, divergence=np.pi / 2),
        DistancePattern(mode="interval", whorl_size=3, divergence=0.4),
        DistancePattern(
            mode="interval",
            spacing=Profile(np.array([0.03, 0.06, 0.10, 0.04])),
            whorl_size=3,
            divergence=0.7,
        ),
        DistancePattern(limit_groups=3),
    )


def check_patterns():
    maximum, cases = 0.0, 0
    lengths = np.r_[0, 0.00001, 0.04, 0.08, 0.1, 0.4, 0.8, 1.2, 2.3]
    for pattern in pattern_cases():
        for first, last in ((0, 1), (0.15, 0.95), (0.5, 0.5), (0.123, 0.876)):
            batch = pattern.tables(lengths, first, last)
            reference = scalar_tables(pattern, lengths, first, last)
            for length, a, b in zip(lengths, batch, reference):
                assert a.shape == b.shape and len(a) <= pattern.capacity(
                    length, first, last
                )
                np.testing.assert_allclose(a, b, atol=2e-13, rtol=0)
                if len(a):
                    maximum = max(maximum, float(abs(a - b).max()))
                    assert first <= a[:, 0].min() <= a[:, 0].max() <= last
                assert not a.flags.writeable
                cases += 1
    for pattern in pattern_cases():
        assert pattern.tables([1], 0, 1, max_sites=0) is None
        assert (
            DistancePattern.from_recipe(pattern.recipe()).recipe() == pattern.recipe()
        )
    varied = replace(pattern_cases()[2], spread_m=0.05)
    a = varied.tables([0.4, 0.8], 0.15, 0.95, seeds=[17, 19])
    b = varied.tables([0.8], 0.15, 0.95, seeds=[19])[0]
    np.testing.assert_array_equal(a[1], b)
    for table in a:
        assert (
            np.isfinite(table).all()
            and table[:, 0].min() >= 0.15
            and table[:, 0].max() <= 0.95
        )
    return {
        "independent_cases": cases,
        "maximum_site_error": maximum,
        "stable_spread": True,
    }


def test_graph():
    pattern = DistancePattern(
        spacing=Profile(np.full(4, 0.09)), whorl_size=2, divergence=np.pi / 2
    )
    zero_variance = Placement(
        pitch=0, pitch_variance=0, scale_variance=0, azimuth_variance=0
    )
    graph = GeneratorGraph(
        17,
        (
            Generator(0, "root", "root", ()),
            Generator(
                1,
                "stems",
                "axis",
                (0,),
                TubeShape(length=0.8),
                replace(
                    zero_variance,
                    count=3,
                    scale=Profile(np.array([0.5, 0.75, 1.0, 1.4])),
                ),
            ),
            Generator(
                2,
                "leaves",
                "blade",
                (1,),
                Blade(length=0.12, width=0.04),
                Placement(pattern=pattern),
            ),
        ),
    )
    return graph


def check_graph(output):
    from graph_products import graph_products

    graph = test_graph()
    capacity = sum(graph.capacities.values())
    assert (
        graph.instances(2, capacity - 1) is None
        and not graph._poses
        and not graph._sites
    )
    leaves = graph.instances(2, capacity)
    actual = len(graph._poses)
    assert actual < capacity and leaves
    lengths = [p.length for p in graph.instances(1, 0)]
    expected = sum(
        len(t) for t in graph.by_id[2].placement.pattern.tables(lengths, 0.15, 0.95)
    )
    assert len(leaves) == expected
    assert all(a is b for a, b in zip(leaves, graph.instances(2, 0)))
    reordered = GeneratorGraph.from_recipe(graph.recipe())
    second = reordered.instances(2, capacity)
    for a, b in zip(leaves, second):
        assert a.path == b.path
        np.testing.assert_array_equal(a.frame, b.frame)
        np.testing.assert_array_equal(a.origin, b.origin)
    near = graph_products(graph, output, "near", max_new_poses=0)
    coarse = graph_products(graph, output, "coarse", max_new_poses=0)
    assert near and coarse and near["plan_identity"] == coarse["plan_identity"]
    with patch("generator_graph.MAX_POSES", 2):
        fresh = test_graph()
        try:
            fresh.instance(((0, 0), (1, 0), (2, 0)))
        except ValueError:
            pass
        else:
            raise AssertionError("invalid direct pose cap accepted")
        assert not fresh._poses and not fresh._sites
    invalid = test_graph()
    try:
        invalid.instance(((0, 0), (1, 0), (2, invalid.slot_capacities[2] - 1)))
    except ValueError:
        pass
    else:
        raise AssertionError("absent site accepted on short parent")
    assert not invalid._poses and not invalid._sites
    bounded = test_graph()
    with patch("generator_graph.MAX_POSES", 3):
        one = bounded.instance(((0, 0), (1, 0), (2, 0)))
        assert len(bounded._poses) == 3 and bounded._site_rows <= 3
        assert bounded.instance(one.path) is one
    return {
        "capacity": capacity,
        "actual_poses": actual,
        "leaf_sites": len(leaves),
        "parent_lengths_m": lengths,
        "warm_new_poses": 0,
        "near_coarse_plan_equal": True,
    }
