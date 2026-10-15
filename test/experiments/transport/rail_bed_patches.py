"""Bounded connected affine contact patches of an explicitly selected finite bed."""

import numpy as np
import shapely


def contact_patches(mesh, *, method="patch", tolerance_m=1e-8):
    if method not in ("patch", "triangle") or not 0 < tolerance_m <= 1e-7:
        raise ValueError("known bounded affine contact method/tolerance required")
    if not 1 <= len(mesh.triangles) <= 8192:
        raise ValueError("affine contact face budget")
    points = mesh.vertices[mesh.triangles]
    shapes = shapely.polygons(points[..., :2])
    if not shapely.coverage_is_valid(shapes):
        raise ValueError("nonoverlapping conforming selected bed required")
    delta = points[:, 1:] - points[:, :1]
    if np.any(np.cross(delta[:, 0], delta[:, 1])[:, 2] <= 0):
        raise ValueError("positive finite height facets required")
    neighbours = [[] for _ in points]
    edges = np.sort(mesh.triangles[:, [[0, 1], [1, 2], [2, 0]]], axis=2)
    _, inverse, counts = np.unique(
        edges.reshape(-1, 2), axis=0, return_inverse=True, return_counts=True
    )
    if np.any(counts > 2):
        raise ValueError("manifold selected bed required")
    order = np.argsort(inverse, kind="stable")
    starts = np.r_[0, np.cumsum(counts)]
    for edge in np.flatnonzero(counts == 2):
        a, b = order[starts[edge] : starts[edge + 1]] // 3
        neighbours[a].append(int(b))
        neighbours[b].append(int(a))
    owner = np.full(len(points), -1, int)
    patches, tests, maximum = [], 0, 0.0
    for seed in range(len(points)):
        if owner[seed] >= 0:
            continue
        gradient = np.linalg.solve(delta[seed, :, :2], delta[seed, :, 2])
        members, queue, tested = [], [seed], set()
        while queue:
            face = queue.pop()
            if face in tested or owner[face] >= 0:
                continue
            tested.add(face)
            tests += 1
            p = points[face]
            height = points[seed, 0, 2] + (p[:, :2] - points[seed, 0, :2]) @ gradient
            error = float(np.max(abs(p[:, 2] - height)))
            if error > tolerance_m:
                continue
            maximum = max(maximum, error)
            owner[face] = len(patches)
            members.append(face)
            if method == "patch":
                queue.extend(neighbours[face])
        domain = shapely.coverage_union_all(shapes[members])
        if domain.is_empty or not domain.is_valid or domain.geom_type != "Polygon":
            raise ValueError("connected finite affine contact patch required")
        patches.append({"plane_face": seed, "faces": sorted(members), "domain": domain})
    return patches, {
        "method": method,
        "faces": len(points),
        "patches": len(patches),
        "plane_face_tests": tests,
        "maximum_plane_error_m": maximum,
        "tolerance_m": tolerance_m,
    }


def cut_to_patches(pieces, patches, *, reach_m=1.35):
    if not 0 < reach_m <= 2 or len(pieces) > 2048 or len(patches) > 8192:
        raise ValueError("bounded patch/source counts and inset required")
    domains = [p["domain"] for p in patches]
    tree = shapely.STRtree(domains)
    output, ledger, tests = [], [], 0
    for index, piece in enumerate(pieces):
        line = shapely.LineString(piece["xy"])
        hits = sorted(tree.query(line, predicate="intersects"))
        tests += len(hits)
        if tests > 65536:
            raise ValueError("affine patch/source intersection budget")
        kept = []
        for hit in hits:
            patch = patches[hit]
            interior = patch["domain"].buffer(-reach_m)
            for part in shapely.get_parts(line.intersection(interior)):
                if part.geom_type != "LineString" or part.length < 0.24:
                    continue
                if len(output) >= 2048:
                    raise ValueError("affine rail piece budget")
                output.append(
                    dict(
                        piece,
                        input_piece=index,
                        patch=int(hit),
                        plane_face=patch["plane_face"],
                        xy=shapely.get_coordinates(part),
                    )
                )
                kept.append(part)
        length = float(shapely.length(kept).sum())
        ledger.append(
            {
                "piece": index,
                "sources": piece["sources"],
                "original_length_m": line.length,
                "candidate_length_m": length,
                "excluded_length_m": max(0.0, line.length - length),
                "reason": "finite_affine_patch_margin_or_short_span"
                if length < line.length - 1e-7
                else "",
            }
        )
    return output, ledger, {"patch_line_candidates": tests}
