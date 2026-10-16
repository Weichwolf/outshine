"""Read actual saved body facets; never infer correctness from solver status."""

import itertools

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import vertical_clearance
from surface_solid_checks import audit_closed


def norm(mesh):
    points = mesh.vertices[mesh.triangles]
    normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    if np.any(normals[:, 2] <= 0):
        raise ValueError("cycle cap has collapsed/reversed projected facet")
    return float(np.linalg.norm(normals[:, :2] / normals[:, 2, None], axis=1).max())


def stair_ranges(data):
    owners = data.get("stair_owners", np.array([0]))
    offsets = data.get("stair_level_offsets", np.array([0, len(data["levels_m"])]))
    if (
        len(owners) != len(set(owners))
        or len(offsets) != len(owners) + 1
        or offsets[0] != 0
        or offsets[-1] != len(data["levels_m"])
        or np.any(np.diff(offsets) < 2)
        or np.any(np.diff(offsets) > 65)
    ):
        raise ValueError("invalid saved stair ownership/level ranges")
    return [
        (int(o), int(a), int(b))
        for o, a, b in zip(owners, offsets[:-1], offsets[1:], strict=True)
    ]


def tread_proof(data, owner=0, start=0, stop=None):
    vertices, top = data["body_vertices"], data["top_faces"]
    chosen, levels = data["face_step"], data["levels_m"][start:stop]
    field = Mesh(
        data["reference_vertices"],
        data["reference_faces"][(data["face_sources"] & (1 << owner)) != 0],
    )
    maximum = norm(field)
    if maximum > 0.5 + 1e-7:
        raise ValueError("actual stair grade envelope exceeds fifty percent")
    footprints, disks = [], []
    for i in range(len(levels)):
        selected = chosen == start + i
        if np.any((data["face_sources"][selected] & (1 << owner)) == 0):
            raise ValueError("saved tread level belongs to another stair source")
        polygons = shapely.polygons(vertices[top[selected], :2])
        shape = shapely.union_all(polygons)
        if shape.geom_type != "Polygon" or shape.is_empty:
            raise ValueError("saved stair tread/landing is split or absent")
        inner = shape.buffer(-0.14)
        if inner.is_empty:
            raise ValueError("saved tread lacks any 0.28-m support disk")
        footprints.append(shape)
        disks.append(float(inner.area))
    for a, b in itertools.pairwise(footprints):
        if a.boundary.intersection(b.boundary).length < 0.5:
            raise ValueError("successive full stair treads lack a connected riser")
    risers = vertices[data["wall_faces"][data["wall_riser"] == 1]]
    if not len(risers) or np.ptp(risers[:, :, 2], axis=1).max() > 0.18 + 1e-8:
        raise ValueError("saved actual stair riser exceeds eighteen centimetres")
    cast = risers.astype(np.float32).astype(float)
    if np.ptp(cast[:, :, 2], axis=1).max() > 0.18:
        raise ValueError("actual Float32 riser exceeds eighteen centimetres")
    return {
        "risers": len(levels) - 1,
        "maximum_envelope_norm": maximum,
        "maximum_float32_riser_m": float(np.ptp(cast[:, :, 2], axis=1).max()),
        "minimum_support_disk_area_m2": min(disks),
        "scope": "Connected actual tread bands, flat landing levels, a 0.28-m disk per band and bounded risers. No complete swept walking body, handrail, statutory stair or outer-route certificate.",
    }


def crossing_faces(data, domains, low, high, point):
    vertices, owners = data["body_vertices"], data["face_sources"]
    top, bottom = data["top_faces"], data["bottom_faces"]
    if point is None:
        return top[(owners & (1 << low)) != 0], bottom[(owners & (1 << high)) != 0]
    regions = [
        p
        for p in shapely.get_parts(domains[low].intersection(domains[high]))
        if p.distance(shapely.Point(point)) < 1e-7 and p.area > 1e-8
    ]
    if len(regions) != 1:
        raise ValueError("one complete real crossing overlap component required")
    a = top[((owners & (1 << low)) != 0) & ((owners & (1 << high)) == 0)]
    b = bottom[((owners & (1 << high)) != 0) & ((owners & (1 << low)) == 0)]
    for faces in (a, b):
        cover = shapely.union_all(shapely.polygons(vertices[faces, :2]))
        if regions[0].difference(cover).area > 1e-7:
            raise ValueError(
                "actual grade crossing was lost or relabelled as a contact"
            )
    return a, b


def audit(data, domains, *, limits=None, crossings=None):
    vertices, faces = data["body_vertices"], data["body_faces"]
    body = Mesh(vertices, faces)
    if not np.array_equal(
        faces,
        np.vstack(
            [data["top_faces"], data["bottom_faces"][:, ::-1], data["wall_faces"]]
        ),
    ):
        raise ValueError("saved body differs from owned caps/walls/risers")
    volume = audit_closed(body)
    cast = vertices.astype(np.float32).astype(float)
    a, b = vertices[faces], cast[faces]
    na = np.cross(a[:, 1] - a[:, 0], a[:, 2] - a[:, 0])
    nb = np.cross(b[:, 1] - b[:, 0], b[:, 2] - b[:, 0])
    if volume <= 0 or np.any(np.einsum("ij,ij->i", na, nb) <= 0):
        raise ValueError("saved stair body volume/actual Float32 orientation invalid")
    top = data["top_faces"]
    owners = data["face_sources"]
    stairs = stair_ranges(data)
    stair_owners = {r[0] for r in stairs}
    cycles, coverage = {}, []
    limits = limits if limits is not None else [0.5, *[0.06] * (len(domains) - 1)]
    for i in range(len(domains)):
        selected = top[(owners & (1 << i)) != 0]
        polygons = shapely.polygons(vertices[selected, :2])
        error = float(shapely.union_all(polygons).symmetric_difference(domains[i]).area)
        if not shapely.coverage_is_valid(polygons) or error > 1e-7:
            raise ValueError("saved source cap lacks its whole original footprint once")
        coverage.append(error)
        if i not in stair_owners:
            norms = [norm(Mesh(v, selected)) for v in (vertices, cast)]
            if max(norms) > limits[i] + 1e-7:
                raise ValueError("actual saved source cap exceeds requested norm")
            cycles[str(i)] = norms
    levels = data["levels_m"]
    for _owner, start, stop in stairs:
        if (
            np.any(np.diff(levels[start:stop]) <= 0)
            or np.diff(levels[start:stop]).max() > 0.18
        ):
            raise ValueError("saved stair riser recipe outside bounds")
    selected = data["face_step"] >= 0
    if np.any(data["face_step"][selected] >= len(levels)):
        raise ValueError("saved tread level is outside its stair recipe")
    expected = levels[data["face_step"][selected]]
    if np.max(abs(vertices[top[selected], 2] - expected[:, None])) > 1e-8:
        raise ValueError("saved stair tread is not at its actual horizontal level")
    if (
        np.max(
            abs(
                vertices[data["bottom_faces"], 2]
                - (data["reference_vertices"][data["reference_faces"], 2] - 0.3)
            )
        )
        > 1e-8
    ):
        raise ValueError("saved stair underside differs from its finite grade envelope")
    separation = {}
    pairs = crossings if crossings is not None else [(1, 2)]
    for pair in pairs:
        low, high = pair[:2]
        point = pair[2] if len(pair) > 2 else None
        low_faces, high_faces = crossing_faces(data, domains, low, high, point)
        proof = {}
        for label, v in (("double", vertices), ("float32", cast)):
            lower = Mesh(v, low_faces)
            upper = Mesh(v, high_faces)
            result = vertical_clearance(lower, upper, max_faces=4096, seconds=5)
            if not result["valid"] or result["minimum_clearance_m"] < 2.5 - 1e-7:
                raise ValueError(
                    "saved whole crossing underside lacks its 2.5-m clearance"
                )
            proof[label] = result
        if crossings is None:
            separation = proof
        else:
            separation[f"{low}:{high}"] = proof
    return {
        "volume_m3": volume,
        (
            "actual_cycle_norms"
            if all(x == 0.06 for x in limits[1:])
            else "actual_profile_norms"
        ): cycles,
        "source_coverage_error_m2": coverage,
        "clearance": separation,
        "treads": (
            tread_proof(data)
            if len(stairs) == 1
            else {str(o): tread_proof(data, o, a, b) for o, a, b in stairs}
        ),
    }
