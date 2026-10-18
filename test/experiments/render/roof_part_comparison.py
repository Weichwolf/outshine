"""Check and render actual post-selection roof-part emission against full sources."""

from collections import Counter
from fractions import Fraction

import numpy as np
import shapely
from building_roof_domains import triangulate_domain
from building_roof_positions import roof_positions
from building_roof_selection import emit_roof, render_models
from outshine_ref.geometry.occlusion import _cross, _sub, classify_boxes


def emit_part(row, sources, recipe):
    local = triangulate_domain(row["piece"])
    original = {
        tuple(point) for point in shapely.get_coordinates(row["piece"]["domain"])
    }
    if any(tuple(point) not in original for point in local[..., :2].reshape(-1, 2)):
        raise ValueError("part triangulation introduced an unbounded contour vertex")
    surface = sources[row["source_index"]]["surface"]
    world = roof_positions(local, row["piece"]["plane_enu"], surface, recipe=recipe)
    if (world < row["bounds"][0]).any() or (world > row["bounds"][1]).any():
        raise ValueError("actual emitted part escapes its pre-emission bound")
    return world


def certificate_contains_points(points, cone):
    for first in range(0, len(points), 512):
        values = points[first : first + 512]
        inside, _ = classify_boxes(np.stack((values, values), axis=1), cone)
        if not inside.all():
            return False
    return True


def actual_part_proof(row, sources, recipe, packet):
    world = emit_part(row, sources, recipe)
    points = world.reshape(-1, 3)
    witness = None
    if len(points):
        for index, (cone, _) in enumerate(packet):
            if certificate_contains_points(points, cone):
                witness = index
                break
        if witness is None:
            raise ValueError("no certificate hides the actual emitted part vertices")
    nondegenerate = 0
    for triangle in world:
        a, b, c = [
            tuple(Fraction(float(value)) for value in point) for point in triangle
        ]
        nondegenerate += any(_cross(_sub(b, a), _sub(c, a)))
    source = sources[row["source_index"]]
    return {
        "source_index": row["source_index"],
        "source_id": source["id"],
        "source_kind": source["kind"],
        "cell": source["cell"],
        "shape": row["shape"],
        "piece_kind": row["kind"],
        "piece_index": row["index"],
        "emitted_triangles": len(world),
        "geometric_nondegenerate_triangles": nondegenerate,
        "witness_certificate": witness,
        "domain_area_m2": row["piece"]["domain"].area,
    }


def part_comparison(
    native, sources, rows, masks, packets, seed_geometry, output, recipe
):
    # Every mask is final before target geometry is emitted. The independent
    # control follows the complete source emitter, including its acceptance.
    proofs = {
        name: [
            actual_part_proof(rows[index], sources, recipe, packets[name])
            for index in np.flatnonzero(mask)
        ]
        for name, mask in masks.items()
    }
    control, omissions = [], Counter()
    for index, source in enumerate(sources):
        triangles, reasons = (
            seed_geometry[index]
            if index in seed_geometry
            else emit_roof(source, vertex_recipe=recipe)
        )
        control.append(triangles)
        omissions.update(reasons)
    models = {"whole": np.concatenate(control)}
    aliases, previous = {}, []
    for name in sorted(masks, key=lambda name: name != "joined"):
        mask = masks[name]
        match = next(
            (old for old in previous if np.array_equal(mask, masks[old])), None
        )
        if match is not None:
            aliases[name] = match
            continue
        selected = [value[0] for value in seed_geometry.values()]
        for index, row in enumerate(rows):
            if not mask[index] and row["source_index"] not in seed_geometry:
                selected.append(emit_part(row, sources, recipe))
        models[name] = np.concatenate(selected)
        previous.append(name)
    output.mkdir()
    return {
        "scope": "Actual roof-part emission after planning, compared with the independent full source roof emitter. No full native/Fine/world or shadow/interaction acceptance.",
        "proofs": proofs,
        "equivalent_masks": aliases,
        "roof_triangles": {name: len(model) for name, model in models.items()},
        "images": render_models(native, sources, models, output),
        "omissions": dict(omissions),
    }
