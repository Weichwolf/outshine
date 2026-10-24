"""Replay the bounded native facet/node bank counterexample, without inferring shores."""

import numpy as np
import shapely


def audit(data):
    records, nodes = data["water"]["sourceSurface344Faces"], data["water"]["points"]
    if not 0 < len(records) <= 2048 or not 0 < len(nodes) <= 4096:
        raise ValueError("bounded existing native slice required")
    triangles = np.array([r["vertices"] for r in records], dtype=float)
    xy = np.array([[r["eastM"], r["northM"]] for r in nodes], dtype=float)
    ids = [r["id"] for r in records]
    if (
        triangles.shape != (len(records), 3, 3)
        or xy.shape != (len(nodes), 2)
        or not np.isfinite(triangles).all()
        or not np.isfinite(xy).all()
        or len(set(ids)) != len(ids)
    ):
        raise ValueError(
            "finite original facets/nodes with unique source face IDs required"
        )
    u, v = triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    normal = np.cross(u, v)
    if np.any(normal[:, 2] <= 0):
        raise ValueError("positive native height facets required")
    polygons = shapely.polygons(triangles[:, :, :2])
    if not shapely.coverage_is_valid(polygons):
        raise ValueError("nonoverlapping conforming native subset required")
    points = shapely.points(xy)
    pairs = shapely.STRtree(polygons).query(points, predicate="covered_by")
    hits = []
    for i, node in enumerate(nodes):
        faces = pairs[1, pairs[0] == i]
        values = []
        for face in faces:
            t, n = triangles[face], normal[face]
            height = t[0, 2] - np.dot(n[:2], xy[i] - t[0, :2]) / n[2]
            values.append(
                {
                    "source_face_id": ids[face],
                    "water_up_m": float(height),
                    "constant_two_m_bed_up_m": float(height - 2),
                }
            )
        hits.append({"node": i, "hits": values, "native_decided_by": node["decidedBy"]})
    domain = shapely.coverage_union_all(polygons)
    report = {
        "native_commit": data["nativeCommit"],
        "source_asset_key": data["source"]["asset_key"],
        "source_faces": len(records),
        "nodes": len(nodes),
        "native_membership_replay": hits,
        "subset_union_area_m2": float(domain.area),
        "unqualified_subset_edge_distances_m": shapely.distance(
            points, domain.boundary
        ).tolist(),
        "shore_role": "unknown: subset boundary is not certified physical shore",
        "scope": "Existing native XY facet attribution and affine ENU bed replay. No complete physical body, original dry DEM reconstruction, fitted height, modified cache or native Float32 proof.",
    }
    return report, triangles, xy
