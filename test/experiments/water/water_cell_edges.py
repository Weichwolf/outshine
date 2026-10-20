"""Shared cell-edge vertices before nonlinear geographic projection or height fits."""

import bisect
import dataclasses
import hashlib
import itertools
import json

import numpy as np
import shapely


def edges(tile):
    z, x, y = tile
    return (
        ((z, 0, x, y), 0, x, 1),
        ((z, 0, x + 1, y), 0, x + 1, 1),
        ((z, 1, x, y), 1, y, 0),
        ((z, 1, x, y + 1), 1, y + 1, 0),
    )


def edge_knots(products, *, knot_limit=8192):
    """Input is the declared edge-owner cohort, never a camera's visible subset.

    Publish the resulting descriptors for overlapping consumer windows. Missing
    cohort cells refuse this preparation; outside-cohort contacts remain open.
    No heights or world-wide completeness are inferred from geometric vertices.
    """
    if type(knot_limit) is not int or not 1 <= knot_limit <= 8192:
        raise ValueError("bounded shared-edge preparation required")
    result = {}
    count = 0
    for tile, product in sorted(products.items()):
        if product.state != "ready" or product.snapshot is None:
            raise ValueError("complete declared edge-owner cohort required")
        for piece in product.snapshot.pieces:
            xy = shapely.get_coordinates(piece.polygon)
            for key, axis, fixed, along in edges(tile):
                values = xy[xy[:, axis] == fixed, along]
                positions = result.setdefault(key, set())
                before = len(positions)
                positions.update(values.tolist())
                count += len(positions) - before
                if count > knot_limit:
                    raise ValueError("shared-edge knot budget exhausted")
    return {key: tuple(sorted(v)) for key, v in sorted(result.items())}


def align_edges(products, knots, *, coordinate_limit=131072):
    """Insert exact published border vertices without changing Mercator segments.

    Each consumer uses the same edge descriptors before projection. Original
    source identities and rings stay in the acquisition inputs. This adds no
    height/contact solution and has no permission to mark missing cells complete.
    """
    if type(coordinate_limit) is not int or not 1 <= coordinate_limit <= 131072:
        raise ValueError("bounded edge-alignment coordinates required")
    count, inserted, results = 0, 0, {}
    for tile, product in sorted(products.items()):
        if product.state != "ready" or product.snapshot is None:
            raise ValueError("complete cell geometry required")
        pieces = []
        digest = hashlib.sha256(
            ("shared-water-edges-v1:" + product.snapshot.digest).encode()
        )
        descriptor = [(key, knots.get(key, ())) for key, *_ in edges(tile)]
        digest.update(json.dumps(descriptor, separators=(",", ":")).encode())
        for piece in product.snapshot.pieces:
            rings = []
            for ring in (piece.polygon.exterior, *piece.polygon.interiors):
                ring_out = []
                xy = np.asarray(ring.coords)
                for a, b in itertools.pairwise(xy):
                    ring_out.append(a)
                    count += 1
                    for key, axis, fixed, along in edges(tile):
                        if a[axis] != fixed or b[axis] != fixed or a[along] == b[along]:
                            continue
                        positions = knots.get(key)
                        if (
                            positions is None
                            or a[along] not in positions
                            or b[along] not in positions
                        ):
                            raise ValueError(
                                "published edge omits an original border endpoint"
                            )
                        lo, hi = sorted((a[along], b[along]))
                        selected = positions[
                            bisect.bisect_right(positions, lo) : bisect.bisect_left(
                                positions, hi
                            )
                        ]
                        if a[along] > b[along]:
                            selected = selected[::-1]
                        for value in selected:
                            point = a.copy()
                            point[along] = value
                            ring_out.append(point)
                            inserted += 1
                            count += 1
                    if count > coordinate_limit:
                        raise ValueError("edge-alignment coordinate budget exhausted")
                rings.append(ring_out)
                count += 1
                if count > coordinate_limit:
                    raise ValueError("edge-alignment coordinate budget exhausted")
            polygon = shapely.normalize(shapely.Polygon(rings[0], rings[1:]))
            if not polygon.is_valid or not polygon.equals(piece.polygon):
                raise ValueError("edge preparation changed source-cell geometry")
            pieces.append(dataclasses.replace(piece, polygon=polygon))
            digest.update(polygon.wkb)
        snapshot = dataclasses.replace(
            product.snapshot, pieces=tuple(pieces), digest=digest.hexdigest()
        )
        results[tile] = dataclasses.replace(product, snapshot=snapshot)
    return results, {
        "output_coordinates": count,
        "inserted_shared_border_vertices": inserted,
    }
