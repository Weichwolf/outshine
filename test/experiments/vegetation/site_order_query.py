"""Transparent fixed counters for an unchanged transport occupancy query."""


class QueryCounter:
    def __init__(self, query):
        self.query = query
        self.shapes, self.tree = query.shapes, query.tree
        self.counts = dict.fromkeys(
            (
                "calls",
                "query_rows",
                "xy_candidates",
                "height_candidates",
                "positive_conflicts",
            ),
            0,
        )

    def conflict(self, footprint, **kwargs):
        self.counts["calls"] += 1
        result, cost = self.query.conflict(footprint, **kwargs)
        for key in self.counts.keys() - {"calls"}:
            self.counts[key] += cost[key]
        return result, cost
