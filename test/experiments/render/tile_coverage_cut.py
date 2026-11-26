"""Reference cut for a resident quadtree shared by terrain and object groups."""

from collections import Counter, deque


def children(tile):
    level, x, y = tile
    return tuple((level + 1, 2 * x + dx, 2 * y + dy) for dy in (0, 1) for dx in (0, 1))

def edge_coordinate(fine_cell, level_gap, sample, segments):
    scale = 1 << level_gap
    return ((fine_cell % scale) * segments + sample) / scale


def complete_tiles(ready):
    """Certify a cell from its resident product or the union of its four quadrants."""
    complete = set(ready)
    pending = deque(complete)
    quadrants = Counter()
    while pending:
        level, x, y = pending.popleft()
        if level == 0:
            continue
        parent = (level - 1, x // 2, y // 2)
        quadrants[parent] += 1
        if quadrants[parent] == 4 and parent not in complete:
            complete.add(parent)
            pending.append(parent)
    return complete


def coverage_cut(roots, ready, refine):
    """Return a non-overlapping resident cut and report roots without complete coverage."""
    complete = complete_tiles(ready)
    selected, missing = [], []

    def visit(tile):
        quadrants = children(tile)
        if tile in ready and (not refine(tile) or not all(q in complete for q in quadrants)):
            selected.append(tile)
            return
        # An implicit parent only enters visit() with a complete partition already certified.
        for quadrant in quadrants:
            visit(quadrant)

    for tile in roots:
        if tile in complete:
            visit(tile)
        else:
            missing.append(tile)
    return selected, missing
