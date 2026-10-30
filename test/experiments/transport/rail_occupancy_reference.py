"""Full actual raised cap scans independent of candidate metadata selection."""

from rail_occupancy_inputs import deck_asset
from stair_occupancy_candidate_reference import PartitionReference
from stair_occupancy_handoff import OccupancyQuery
from stair_occupancy_inputs import road_volumes, volume


def whole_raised(products, *, cast=False):
    rows, owners, _error = road_volumes(
        (deck_asset(products, cast=cast),), products.originals
    )
    rows, owners = list(rows), list(owners)
    identity = products.support_identity + (":f32" if cast else ":f64")
    for index, (low, high) in enumerate(products.contacts):
        lower, upper = low.vertices, high.vertices
        if cast:
            lower, upper = (
                lower.astype("f4").astype(float),
                upper.astype("f4").astype(float),
            )
        for face, triangle in enumerate(high.triangles):
            row, _error = volume(
                f"{identity}:support:{index}:face:{face}",
                lower[triangle],
                upper[triangle],
                0,
            )
            rows.append(row)
            owners.append((products.owners[index],))
    return tuple(rows), tuple(owners)


class RailReference:
    def __init__(self, baseline, products):
        old = PartitionReference(baseline, products.state, products.originals)
        extra, owners = whole_raised(products)
        self.rows = (*old.rows, *extra)
        self.owners = (*old.owners, *owners)
        self.by_id = {
            row.owner_id: (row, keys)
            for row, keys in zip(self.rows, self.owners, strict=True)
        }
        if len(self.by_id) != len(self.rows):
            raise ValueError("Whole raised reference identity must remain unique")
        # Keep the old partitions; never increase the flat8192-volume capacity.
        self.queries = (*old.queries, OccupancyQuery(extra))

    def conflict(self, footprint, **kwargs):
        import shapely

        return shapely.union_all(
            [q.conflict(footprint, **kwargs)[0] for q in self.queries]
        ), {}
