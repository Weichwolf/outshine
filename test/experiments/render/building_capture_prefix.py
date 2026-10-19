"""Replay the unchanged capture hierarchy only until its first eligible source.

No closer surface hit exists before that callback. Thus its expensive part loop
is an original-work lower bound, independent of later geometry/occlusion hits.
This is diagnostic replay, not a replacement world tracer or renderer.
"""

import numpy as np

NODE = np.dtype([("bounds", "<f8", (2, 3)), ("links", "<u4", (4,))])


def capture_tree(bounds):
    """Same unique midpoint/axis/index partition as native PlanHierarchy::Build.

    Sorting replaces nth_element solely in this reference: the native comparator
    resolves equal centres by local index, uniquely fixing every child set.
    Input coordinates are bounded so midpoint's safe (a+b)/2 branch applies.
    """
    bounds = np.asarray(bounds, dtype=np.float64)
    if (
        bounds.ndim != 3
        or bounds.shape[1:] != (2, 3)
        or not 1 <= len(bounds) <= 65536
        or not np.isfinite(bounds).all()
        or (bounds[:, 0] > bounds[:, 1]).any()
        or np.abs(bounds).max() > 1e7
    ):
        raise ValueError("bounded finite original native source boxes required")
    order = np.arange(len(bounds), dtype=np.uint32)
    nodes = np.zeros(2 * len(bounds) - 1, dtype=NODE)
    centres = (bounds[:, 0] + bounds[:, 1]) / 2
    next_node = 0

    def partition(first, count):
        nonlocal next_node
        at, next_node = next_node, next_node + 1
        members = order[first : first + count]
        nodes[at]["bounds"] = (
            bounds[members, 0].min(axis=0),
            bounds[members, 1].max(axis=0),
        )
        nodes[at]["links"][:2] = (first, count)
        if count != 1:
            span = np.ptp(centres[members], axis=0)
            axis = int(np.argmax(span))
            order[first : first + count] = members[
                np.lexsort((members, centres[members, axis]))
            ]
            half = count // 2
            left = partition(first, half)
            right = partition(first + half, count - half)
            nodes[at]["links"][2:] = (left, right)
        return at

    partition(0, len(bounds))
    nodes.setflags(write=False)
    order.setflags(write=False)
    return nodes, order


def native_tree(snapshot):
    """Decode the verified frozen x86-64/libc++ tree snapshot; keep bytes owned."""
    count = snapshot["count"]
    nodes = np.frombuffer(bytes.fromhex(snapshot["nodes_hex"]), dtype=NODE)
    order = np.frombuffer(bytes.fromhex(snapshot["order_hex"]), dtype="<u4")
    bounds = np.frombuffer(bytes.fromhex(snapshot["bounds_hex"]), dtype="<f8").reshape(
        -1, 2, 3
    )
    if len(bounds) != count:
        raise ValueError("native source count mismatch")
    expected_nodes, expected_order = capture_tree(bounds)
    if (
        nodes.tobytes() != expected_nodes.tobytes()
        or order.tobytes() != expected_order.tobytes()
    ):
        raise ValueError("reference tree differs from actual native node/order bytes")
    return nodes, order, bounds


def ray_entry(bounds, origins, directions):
    """Literal native RayEntry with minimum=0, maximum=infinity, including zeros.

    Return validity separately: an infinite native entry is not a nullopt.
    Child pushes still require finite entries in the original traversal.
    """
    origins, directions = np.broadcast_arrays(origins, directions)
    count = len(directions)
    low, high, valid = np.zeros(count), np.full(count, np.inf), np.ones(count, bool)
    with np.errstate(divide="ignore", invalid="ignore", over="ignore"):
        for axis in range(3):
            direction = directions[:, axis]
            moving = direction != 0
            first = np.divide(
                bounds[..., 0, axis] - origins[:, axis],
                direction,
                out=np.full(count, -np.inf),
                where=moving,
            )
            last = np.divide(
                bounds[..., 1, axis] - origins[:, axis],
                direction,
                out=np.full(count, np.inf),
                where=moving,
            )
            low = np.maximum(low, np.minimum(first, last))
            high = np.minimum(high, np.maximum(first, last))
            valid &= moving | (
                (origins[:, axis] >= bounds[..., 0, axis])
                & (origins[:, axis] <= bounds[..., 1, axis])
            )
    return valid & (low <= high), low


def first_source(nodes, order, projectable, origin, directions):
    """Original sorted DFS, stopping before the first expensive source callback.

    Equal child entries push left then right, hence visit right first, exactly
    as native TraceClosest. No source-hit or opacity model is introduced.
    """
    directions = np.asarray(directions, dtype=np.float64)
    if (
        directions.ndim != 2
        or directions.shape[1:] != (3,)
        or len(directions) > 65536
        or not np.isfinite(directions).all()
        or not np.isfinite(origin).all()
        or not (directions != 0).any(axis=1).all()
    ):
        raise ValueError("bounded finite moving-ray batch required")
    projectable = np.asarray(projectable, dtype=bool)
    if len(projectable) != len(order):
        raise ValueError("original source projection flags required")
    count = len(directions)
    origins = np.broadcast_to(origin, directions.shape)
    pending = np.zeros((count, 32), dtype=np.uint32)
    root_hit, _ = ray_entry(nodes[0]["bounds"], origins, directions)
    depth = root_hit.astype(np.int32)
    answer = np.full(count, -1, dtype=np.int64)
    tests = np.ones(count, dtype=np.uint32)
    while True:
        active = np.flatnonzero((depth != 0) & (answer < 0))
        if not len(active):
            break
        depth[active] -= 1
        at = pending[active, depth[active]]
        links = nodes["links"][at]
        leaves = links[:, 1] == 1
        leaf_rays = active[leaves]
        sources = order[links[leaves, 0]]
        eligible = projectable[sources]
        answer[leaf_rays[eligible]] = sources[eligible]
        active, links = active[~leaves], links[~leaves]
        tests[active] += 2
        children = links[:, 2:].copy()
        entries = np.full((len(active), 2), np.inf)
        for side in range(2):
            hit, along = ray_entry(
                nodes["bounds"][children[:, side]],
                origins[active],
                directions[active],
            )
            entries[:, side] = np.where(hit, along, np.inf)
        swap = entries[:, 0] < entries[:, 1]
        children[swap] = children[swap, ::-1]
        entries[swap] = entries[swap, ::-1]
        for side in range(2):
            valid = np.isfinite(entries[:, side])
            rays = active[valid]
            if (depth[rays] >= pending.shape[1]).any():
                raise ValueError("original balanced-tree stack capacity exceeded")
            pending[rays, depth[rays]] = children[valid, side]
            depth[rays] += 1
    return answer, tests
