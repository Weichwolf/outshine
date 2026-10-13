"""Material-only taper of terminal chains, respecting complete-network junctions."""

import numpy as np
import shapely


def terminal_profile(
    positions, parents, radii, network_positions, network_edges, minor_radius
):
    positions, parents, radii = (
        np.asarray(positions, float),
        np.asarray(parents),
        np.asarray(radii, float),
    )
    if (
        not 1 <= len(positions) <= 2048
        or positions.shape != (len(parents), 2)
        or radii.shape != parents.shape
    ):
        raise ValueError("bounded matching primary tree arrays required")
    if (
        parents[0] != -1
        or np.any(parents[1:] < 0)
        or np.any(parents[1:] >= np.arange(1, len(parents)))
    ):
        raise ValueError("parent-before-child tree required")
    if (
        not np.isfinite(positions).all()
        or not np.isfinite(radii).all()
        or np.any(radii <= 0)
        or not np.isfinite(minor_radius)
        or minor_radius <= 0
    ):
        raise ValueError("finite physical tree and positive radii required")
    lookup = {tuple(point): i for i, point in enumerate(network_positions)}
    degree = np.bincount(
        np.asarray(network_edges).ravel(), minlength=len(network_positions)
    )
    primary_degree = np.array([degree[lookup[tuple(point)]] for point in positions])
    children = np.bincount(parents[1:], minlength=len(parents))
    tips = np.flatnonzero(children == 0)
    tips = tips[(tips != 0) & (primary_degree[tips] <= 2)]
    protected = interior_junction_edges(
        positions, parents, np.asarray(network_positions)[degree > 2]
    )
    output, chains = radii.copy(), []
    for tip in tips:
        chain, distances = [int(tip)], [0.0]
        node = int(tip)
        while node != 0:
            if node in protected:
                break
            parent = int(parents[node])
            distances.append(
                distances[-1]
                + float(np.linalg.norm(positions[node] - positions[parent]))
            )
            chain.append(parent)
            node = parent
            if primary_degree[node] != 2 or children[node] != 1:
                break
        if len(chain) == 1:
            continue
        length = min(8 * radii[tip], distances[-1])
        if length <= 0:
            raise ValueError("nondegenerate terminal chain required")
        target = (
            min(radii[tip], minor_radius)
            if primary_degree[tip] > 1
            else 0.05 * radii[tip]
        )
        t = np.clip(np.asarray(distances) / length, 0, 1)
        weight = t * t * (3 - 2 * t)
        rows = np.asarray(chain)[t < 1]
        output[rows] = radii[rows] + (target - radii[rows]) * (1 - weight[t < 1])
        output[tip] = target
        chains.append(
            {
                "tip": int(tip),
                "attached": bool(primary_degree[tip] > 1),
                "nodes": chain,
                "length_m": float(length),
                "target_radius_m": float(target),
            }
        )
    assert np.array_equal(output[primary_degree > 2], radii[primary_degree > 2])
    assert output[0] == radii[0] and np.all(output > 0) and np.all(output <= radii)
    return output, chains


def interior_junction_edges(positions, parents, junctions):
    if len(positions) < 2 or not len(junctions):
        return set()
    a, b = positions[parents[1:]], positions[1:]
    tree = shapely.STRtree(shapely.points(junctions))
    pairs = tree.query(
        shapely.linestrings(np.stack((a, b), axis=1)),
        predicate="dwithin",
        distance=1e-12,
    )
    return {
        int(edge + 1)
        for edge, point in pairs.T
        if not np.array_equal(junctions[point], a[edge])
        and not np.array_equal(junctions[point], b[edge])
    }
