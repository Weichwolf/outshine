"""Declared longitudinal/crossfall alternative; whole-norm remains the default."""

import math
from itertools import pairwise

import numpy as np
import shapely
from stair_junction_solve import facets, gradient_rows


def checked_crossfall(value):
    if not math.isfinite(value) or not 0.002 < value <= 0.2:
        raise ValueError("bounded explicit procedural crossfall limit required")


def directional_facets(
    plan,
    system,
    directions,
    reserve,
    flat,
    precision,
    *,
    height_bound_m,
    crossfall_limit,
):
    checked_crossfall(crossfall_limit)
    stairs = {s[0] for s in plan["stairs"]}
    stair_plan = dict(
        plan,
        parts=[
            f if i in stairs else np.empty((0, 3), int)
            for i, f in enumerate(plan["parts"])
        ],
    )
    facets(
        stair_plan,
        system,
        directions,
        reserve,
        flat,
        precision,
        height_bound_m=height_bound_m,
    )
    cast = plan["xy"].astype(np.float32).astype(float)
    epsilon = float(np.spacing(np.float32(height_bound_m))) / 2
    work = {"segment_face_candidates": 0, "directional_faces": 0, "directional_rows": 0}
    for owner, (row, faces) in enumerate(zip(plan["rows"], plan["parts"], strict=True)):
        if owner in stairs:
            continue
        shapes = shapely.polygons(plan["xy"][faces])
        covered = np.zeros(len(faces), bool)
        for a, b in pairwise(row["xy"]):
            tangent = (b - a) / np.linalg.norm(b - a)
            normal = np.array([-tangent[1], tangent[0]])
            vectors = np.array([tangent, -tangent, normal, -normal])
            influence = shapely.LineString([a, b]).buffer(row["width_m"])
            work["segment_face_candidates"] += len(faces)
            hit = np.flatnonzero(
                shapely.area(shapely.intersection(shapes, influence)) > 1e-12
            )
            covered[hit] = True
            limits = np.array(
                [row["gradient"], row["gradient"], crossfall_limit, crossfall_limit]
            )
            margin = np.where(limits > 0.1, 0.002, 0.0002)
            for index in hit:
                face = faces[index]
                work["directional_faces"] += 1
                for values, bound in zip(
                    vectors @ gradient_rows(plan["xy"], face),
                    limits - margin,
                    strict=True,
                ):
                    system.add(face, values, bound)
                    work["directional_rows"] += 1
                if precision:
                    for values, limit in zip(
                        vectors @ gradient_rows(cast, face), limits, strict=True
                    ):
                        bound = limit - epsilon * float(abs(values).sum())
                        if bound < 0:
                            raise ValueError(
                                "directional path face requires a better conditioned precision mesh"
                            )
                        system.add(face, values, bound)
                        work["directional_rows"] += 1
        if not covered.all():
            raise ValueError(
                "directional grade leaves a source face without an axis segment"
            )
    return work
