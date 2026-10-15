"""Local optional-hardware admission; touching rail feet/sleeper tops are contacts."""

import numpy as np
import shapely


def admit_pieces(products, rows):
    if len(products) != len(rows):
        raise ValueError("one successful hardware row per candidate product required")
    shapes, owners, kinds = [], [], []
    for index, product in enumerate(products):
        count = len(product.footprints)
        shapes.extend(product.footprints)
        owners.extend([index] * count)
        kinds.extend([0, 0] + [1] * (count - 2))
    shapes, owners, kinds = np.array(shapes, object), np.array(owners), np.array(kinds)
    tree, pairs_seen, conflicts = shapely.STRtree(shapes), 0, set()
    for begin in range(0, len(shapes), 64):
        pairs = tree.query(shapes[begin : begin + 64], predicate="intersects")
        pairs_seen += pairs.shape[1]
        if pairs_seen > 1000000:
            for row in rows:
                row.update(valid=False, reason="rail_admission_pair_budget")
            return [], {
                "admission_pair_candidates": pairs_seen,
                "rejected_conflicting_pieces": len(rows),
            }
        a, b = begin + pairs[0], pairs[1]
        selected = (a < b) & (owners[a] != owners[b]) & (kinds[a] == kinds[b])
        a, b = a[selected], b[selected]
        overlaps = shapely.area(shapely.intersection(shapes[a], shapes[b])) > 1e-9
        for first, second in zip(owners[a[overlaps]], owners[b[overlaps]], strict=True):
            conflicts.add(tuple(sorted((int(first), int(second)))))
    neighbours = [set() for _ in products]
    for first, second in conflicts:
        neighbours[first].add(second)
        neighbours[second].add(first)
    accepted = set()
    for index, row in enumerate(rows):
        blocking = sorted(accepted.intersection(neighbours[index]))
        if blocking:
            row.update(
                valid=False,
                reason="rail_hardware_piece_conflict",
                conflicts_with_pieces=[rows[i]["piece"] for i in blocking],
                retained_existing_bed=True,
            )
        else:
            accepted.add(index)
    return [product for i, product in enumerate(products) if i in accepted], {
        "admission_pair_candidates": pairs_seen,
        "conflict_pairs": len(conflicts),
        "rejected_conflicting_pieces": len(products) - len(accepted),
    }
