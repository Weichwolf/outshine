"""Union actual closed roof geometry with declared metrical dormer volumes."""

from dataclasses import replace
from types import MappingProxyType

import manifold3d as manifold
import numpy as np
import shapely
from outshine_ref.buildings.modelling.prism_cut import decoded, encoded, prism_source
from outshine_ref.buildings.modelling.prism_union import PrismVolume, frozen


def union(envelope, dormers, *, method="batch", maximum=65536):
    if method not in ("batch", "sequential") or not dormers or len(dormers) > 64:
        raise ValueError("Known union method and1..64 dormers required")
    if type(maximum) is not int or not 12 <= maximum <= 131072:
        raise ValueError("Explicit finite triangle capacity12..131072 required")
    if len({p.identifier for p in dormers}) != len(dormers):
        raise ValueError("Dormer identifiers must be unique")
    sources = tuple(
        PrismVolume(
            p.identifier, shapely.from_wkb(p.footprint_wkb), p.low_m, p.cap_m, 1
        )
        for p in dormers
    )
    mesh, roots = envelope.mesh, envelope.vertex_roots
    footprint = shapely.from_wkb(envelope.footprint_wkb)
    if any(
        not footprint.covers(p.polygon)
        or p.low_m < envelope.base_m
        or p.high_m > float(mesh["vertices"][:, 2].max())
        for p in sources
    ):
        raise ValueError("Dormer leaves the source footprint or height interval")
    if len(mesh["faces"]) + len(sources) * 12 > maximum:
        raise ValueError("Dormer source triangle capacity exceeded")
    points = mesh["vertices"]
    origin = np.r_[points[:, :2].min(axis=0) + np.ptp(points[:, :2], axis=0) / 2, 0.0]
    identifiers = manifold.Manifold.reserve_ids(len(sources) + 1)
    ids = {identifiers + i: i for i in range(len(sources) + 1)}
    original = encoded(mesh, roots, origin, identifiers, ("uv",))
    counts, operands = [len(mesh["faces"])], [original]
    for index, source in enumerate(sources, 1):
        solid, count = prism_source(source, origin, identifiers + index, ("uv",))
        operands.append(solid)
        counts.append(count)
    if method == "batch":
        result = manifold.Manifold.batch_boolean(operands, manifold.OpType.Add)
    else:
        result = original
        for operand in operands[1:]:
            result = result + operand
    if result.status() != manifold.Error.NoError:
        raise ValueError("Dormer union failed: " + str(result.status()))
    output = decoded(
        result,
        mesh,
        sources,
        ("original-roof", *(p.identifier for p in sources)),
        ids,
        counts,
        origin,
        ("uv",),
        maximum,
        (),
    )
    changed = {k: np.array(v, copy=True) for k, v in output.mesh.items()}
    selected = output.source_indices > 0
    canopy = selected & (changed["normals"][changed["faces"][:, 0], 2] > 0.999999)
    changed["materials"][canopy] = 9
    for index, p in enumerate(dormers, 1):
        mask = output.source_indices == index
        for face in changed["faces"][mask]:
            n = changed["normals"][face[0]]
            u = np.r_[(np.asarray(p.b) - p.a) / p.width_m, 0.0]
            if abs(u @ n) > 0.999999:
                u = np.array([-u[1], u[0], 0.0])
            v = np.cross(n, u)
            positions = changed["vertices"][face] - np.r_[p.a, p.low_m]
            changed["uv"][face] = positions @ np.array([u, v]).T
    return replace(
        output, mesh=MappingProxyType({k: frozen(v) for k, v in changed.items()})
    )
