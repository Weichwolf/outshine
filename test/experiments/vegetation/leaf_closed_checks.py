"""Tagged-arrival, contour, topology and identical-growth checks."""

import numpy as np
from blade_surface import Blade
from leaf_closed_state import SourceState
from leaf_closed_venation import grow_closed
from leaf_domain import contains_segment
from leaf_network import node_network, topology


def same_network(first, second):
    for name in ("positions", "edges", "hubs", "first_reach", "completed"):
        assert np.array_equal(getattr(first, name), getattr(second, name)), name
    for name in ("remaining_sources", "stop_reason", "rejected_segments"):
        assert getattr(first, name) == getattr(second, name), name


def closed_checks():
    blade = Blade(lobes=0)
    positions = np.array([[-0.006, 0.06], [0, 0.04], [0.012, 0.06]])
    edges = np.array([[0, 1], [1, 2]])
    sources = np.array([[0.0, 0.06]])
    dense = grow_closed(blade, positions, edges, sources, "dense")
    sectors = grow_closed(blade, positions, edges, sources)
    same_network(dense, sectors)
    assert sectors.remaining_sources == 0
    assert sectors.completed[0] > sectors.first_reach[0]
    graph = topology(sectors.positions, sectors.edges)
    assert graph["components"] == 1 and graph["cycles"] == 2
    tags = SourceState(sources[0], tags={0, 2})
    tags.inherit({0: 4, 2: 5})
    assert tags.tags == {4, 5}
    partial = grow_closed(blade, positions, edges, sources, steps=0)
    assert partial.remaining_sources == 1 and partial.stop_reason == "step_budget"
    partial = grow_closed(blade, positions, edges, sources, max_nodes=3)
    assert partial.stop_reason == "node_budget" and len(partial.positions) == 3
    try:
        grow_closed(blade, np.array([[1.0, 0.06]]), np.empty((0, 2), int), sources)
    except ValueError:
        pass
    else:
        raise AssertionError("outside isolated seed accepted")
    crossing = np.array([[-1.0, -1], [1.0, 1], [-1.0, 1], [1.0, -1]])
    p, e, crossing_graph = node_network(crossing, np.array([[0, 1], [2, 3]]))
    assert len(p) == 5 and len(e) == 4 and crossing_graph["components"] == 1
    lobed = Blade(lobes=4, lobe_depth=0.7)
    t = np.linspace(0.15, 0.85, 29)
    width, _ = lobed.width_profile(t)
    margin = np.column_stack((width * 0.98, t * lobed.length))
    escaped = 0
    for a, b in zip(margin[:-4], margin[4:]):
        mid = (a + b) / 2
        w, _ = lobed.width_profile(mid[1] / lobed.length)
        if abs(mid[0]) > w:
            assert not contains_segment(lobed, a, b)
            escaped += 1
    assert escaped > 0
    return {
        "asymmetric_arrival": {
            "first": int(sectors.first_reach[0]),
            "completed": int(sectors.completed[0]),
            **graph,
        },
        "tags_move_to_descendants": True,
        "crossing_creates_shared_junction": True,
        "escaped_chords_rejected": escaped,
        "explicit_step_and_node_budgets": True,
    }
