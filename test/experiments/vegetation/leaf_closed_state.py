"""Tagged source lifetime and bounded shared junctions for closed venation."""

from dataclasses import dataclass

import numpy as np
from leaf_domain import contains_segment


class NetworkBudget(Exception):
    pass


class NetworkBuilder:
    def __init__(self, blade, positions, edges, max_nodes, max_edges):
        self.blade = blade
        self.positions = [np.array(p, dtype=float) for p in positions]
        self.edges = [tuple(sorted(map(int, e))) for e in edges]
        self.keys = {tuple(p): i for i, p in enumerate(self.positions)}
        self.edge_set = set(self.edges)
        self.max_nodes, self.max_edges = max_nodes, max_edges
        self.rejected_segments = 0

    def connect(self, parent, point):
        first = self.positions[parent]
        if not contains_segment(self.blade, first, point):
            self.rejected_segments += 1
            return None
        key = tuple(point)
        child = self.keys.get(key, len(self.positions))
        edge = tuple(sorted((parent, child)))
        if parent == child or edge in self.edge_set:
            return child
        if len(self.edges) == self.max_edges:
            raise NetworkBudget("edge_budget")
        if child == len(self.positions):
            if child == self.max_nodes:
                raise NetworkBudget("node_budget")
            self.keys[key] = child
            self.positions.append(np.array(point))
        self.edges.append(edge)
        self.edge_set.add(edge)
        return child


@dataclass
class SourceState:
    point: np.ndarray
    tags: set | None = None
    active: bool = True
    hub: int = -1
    first_reach: int = -1
    completed: int = -1

    def influence(self, owners, builder, kill, iteration):
        owners = set(map(int, owners))
        near = {
            i
            for i in owners
            if np.linalg.norm(builder.positions[i] - self.point) <= kill
        }
        if near and self.tags is None:
            self.tags = owners.copy()
            self.first_reach = iteration
        if self.tags is not None:
            self.tags.intersection_update(owners)
            for node in sorted(near & self.tags):
                child = builder.connect(node, self.point)
                if child is not None:
                    self.hub = child
                    self.tags.remove(node)
            if not self.tags:
                self.active = False
                self.completed = iteration
                return []
        return sorted(owners - near)

    def inherit(self, children):
        if self.active and self.tags is not None:
            self.tags = {children.get(i, i) for i in self.tags}
