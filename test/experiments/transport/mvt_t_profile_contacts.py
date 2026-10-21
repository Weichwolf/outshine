"""Explicit source-owned fixed points/whole lines, never nearest-height snapping."""

import time
from math import isfinite

import numpy as np
import shapely
from stair_module_sections import interpolate, line_pieces


def _checked(plan, contacts, max_samples, method):
    if (
        method not in ("index", "scan")
        or type(max_samples) is not int
        or not 0 <= max_samples <= 200000
    ):
        raise ValueError("known search and bounded integer contact-sample cap required")
    if len(contacts) > 64 or sum(len(c["xyz_m"]) for c in contacts) > 4096:
        raise ValueError("fixed contact input exceeds declared structural caps")
    owners = {row["source_id"]: i for i, row in enumerate(plan["rows"])}
    result, seen = [], set()
    for contact in contacts:
        key = contact["contact_id"]
        if not isinstance(key, str) or not 1 <= len(key) <= 512 or key in seen:
            raise ValueError("unique bounded contact identities required")
        seen.add(key)
        if (
            contact.get("binding") != "fixed"
            or not isinstance(contact.get("provenance"), str)
            or not 1 <= len(contact["provenance"]) <= 2048
        ):
            raise ValueError("explicit fixed intent and bounded provenance required")
        if contact["source_id"] not in owners:
            raise ValueError("fixed contact must name an actual retained source part")
        xyz = np.array(contact["xyz_m"], dtype=float)
        if (
            xyz.ndim != 2
            or xyz.shape[1:] != (3,)
            or not 1 <= len(xyz) <= 128
            or not np.isfinite(xyz).all()
            or abs(xyz).max() > 1e7
        ):
            raise ValueError("bounded finite fixed point or whole XYZ profile required")
        if len(xyz) > 1:
            lengths = np.linalg.norm(np.diff(xyz[:, :2], axis=0), axis=1)
            if np.any(lengths == 0) or np.any(
                np.diff(np.r_[0, np.cumsum(lengths)]) <= 0
            ):
                raise ValueError(
                    "distinct representable consecutive XY profile stations required"
                )
        if len(xyz) > 1 and not shapely.LineString(xyz[:, :2]).is_simple:
            raise ValueError("one simple source-owned height profile required")
        result.append((contact, owners[contact["source_id"]], xyz))
    return result


def _weights(plan, faces, xy):
    triangles = plan["xy"][faces]
    values = np.broadcast_to(np.eye(3), (len(faces), 3, 3))
    return np.stack(
        [interpolate(triangles, xy, values[:, :, i]) for i in range(3)], axis=1
    )


def _samples(plan, owner, xyz, polygons, tree, work, remaining, max_candidates):
    geometry = (
        shapely.Point(xyz[0, :2]) if len(xyz) == 1 else shapely.LineString(xyz[:, :2])
    )
    if not plan["prepared_domains"][owner].covers(geometry):
        raise ValueError("fixed contact leaves its source's prepared surface")
    faces = plan["parts"][owner]

    def query(shape):
        indices = (
            sorted(tree.query(shape).tolist())
            if tree is not None
            else list(range(len(faces)))
        )
        if len(indices) > max_candidates - work["candidate_faces"]:
            raise ValueError("fixed contact candidate budget exhausted before binding")
        work["queries"] += 1
        work["candidate_faces"] += len(indices)
        return np.array(indices, dtype=int)

    points, parents = [], []
    if len(xyz) > 1:
        candidates = query(geometry)
        pieces, parent = line_pieces(
            plan["xy"], faces[candidates], xyz[:, :2], minimum_length_m=0
        )
        size = int(shapely.get_num_coordinates(pieces).sum())
        if size > remaining:
            raise ValueError("fixed contact sample budget exhausted before binding")
        xy, piece = shapely.get_coordinates(pieces, return_index=True)
        points.append(xy)
        parents.append(candidates[parent[piece]])
        remaining -= size
    for point in xyz[:, :2]:
        candidates = query(shapely.Point(point))
        selected = candidates[
            shapely.covers(polygons[candidates], shapely.Point(point))
        ]
        if not len(selected):
            raise ValueError("fixed profile knot has no source-owned facet")
        if len(selected) > remaining:
            raise ValueError("fixed contact sample budget exhausted before binding")
        points.append(np.repeat(point[None], len(selected), axis=0))
        parents.append(selected)
        remaining -= len(selected)
    xy, parent = np.concatenate(points), np.concatenate(parents)
    if len(xyz) == 1:
        target = np.full(len(xy), xyz[0, 2])
    else:
        arc = np.r_[0, np.cumsum(np.linalg.norm(np.diff(xyz[:, :2], axis=0), axis=1))]
        target = np.interp(
            shapely.line_locate_point(geometry, shapely.points(xy)), arc, xyz[:, 2]
        )
    return faces[parent], xy, target


def prepare_contacts(
    plan, contacts, *, max_samples=32768, max_candidates=200000, method="index"
):
    began = time.perf_counter()
    if type(max_candidates) is not int or not 0 <= max_candidates <= 2000000:
        raise ValueError("bounded integer contact-candidate cap required")
    checked = _checked(plan, contacts, max_samples, method)
    work = {
        "contacts": len(checked),
        "queries": 0,
        "candidate_faces": 0,
        "samples": 0,
        "index_entries": 0,
        "index_build_s": 0.0,
        "geometry_prepare_s": 0.0,
    }
    indices, prepared = {}, []
    for contact, owner, xyz in checked:
        if owner not in indices:
            began_source = time.perf_counter()
            polygons = shapely.polygons(plan["xy"][plan["parts"][owner]])
            polygons_ready = time.perf_counter()
            tree = shapely.STRtree(polygons) if method == "index" else None
            work["geometry_prepare_s"] += polygons_ready - began_source
            work["index_build_s"] += (
                time.perf_counter() - polygons_ready if tree is not None else 0.0
            )
            indices[owner] = polygons, tree
            work["index_entries"] += len(polygons) if tree is not None else 0
        polygons, tree = indices[owner]
        faces, xy, target = _samples(
            plan,
            owner,
            xyz,
            polygons,
            tree,
            work,
            max_samples - work["samples"],
            max_candidates,
        )
        weights = _weights(plan, faces, xy)
        work["samples"] += len(xy)
        prepared.append(
            {
                "contact_id": contact["contact_id"],
                "source_id": contact["source_id"],
                "owner": owner,
                "faces": faces,
                "xy": xy,
                "weights": weights,
                "target_z_m": target,
                "provenance": contact["provenance"],
            }
        )
    return prepared, {
        **work,
        "elapsed_s": time.perf_counter() - began,
        "coefficient_array_bytes": sum(
            p["faces"].nbytes
            + p["weights"].nbytes
            + p["xy"].nbytes
            + p["target_z_m"].nbytes
            for p in prepared
        ),
    }


def add_contacts(system, prepared, datum, *, row_scale=1024.0):
    if not isfinite(row_scale) or not 0 < row_scale <= 65536:
        raise ValueError("finite positive bounded contact row scale required")
    for contact in prepared:
        for face, weights, height in zip(
            contact["faces"], contact["weights"], contact["target_z_m"], strict=True
        ):
            system.equal(face, weights * row_scale, (height - datum) * row_scale)


def contact_residuals(prepared, heights):
    return {
        contact["contact_id"]: float(
            abs(
                np.einsum("ij,ij->i", heights[contact["faces"]], contact["weights"])
                - contact["target_z_m"]
            ).max()
        )
        for contact in prepared
    }


def audit_fixed_contacts(plan, heights, contacts, *, max_samples=32768):
    from mvt_t_profile_products import whole_section
    from outshine_ref.contracts import Mesh
    from outshine_ref.geometry import SurfaceSampler

    checked = _checked(plan, contacts, max_samples, "scan")
    samplers, errors, count = {}, {}, 0
    for contact, owner, xyz in checked:
        geometry = (
            shapely.Point(xyz[0, :2])
            if len(xyz) == 1
            else shapely.LineString(xyz[:, :2])
        )
        if not plan["prepared_domains"][owner].covers(geometry):
            raise ValueError("fixed contact audit leaves its retained source surface")
        if owner not in samplers:
            samplers[owner] = SurfaceSampler(
                Mesh(np.c_[plan["xy"], heights], plan["parts"][owner])
            )
        sampler = samplers[owner]
        if len(xyz) == 1:
            result = sampler.sample(xyz[:, :2])
            if any(result.reasons):
                raise ValueError("fixed source point has no unique height surface")
            error = float(abs(result.points_enu_m[0, 2] - xyz[0, 2]))
            count += 1
        else:
            line = shapely.LineString(xyz[:, :2])
            actual = np.array(
                whole_section(
                    line,
                    shapely.polygons(plan["xy"][plan["parts"][owner]]),
                    sampler,
                    xyz[:, :2],
                )
            )
            arc = np.r_[
                0, np.cumsum(np.linalg.norm(np.diff(xyz[:, :2], axis=0), axis=1))
            ]
            target = np.interp(
                shapely.line_locate_point(line, shapely.points(actual[:, :2])),
                arc,
                xyz[:, 2],
            )
            error = float(abs(actual[:, 2] - target).max())
            count += len(actual)
        if count > max_samples:
            raise ValueError("fixed contact audit sample budget exhausted")
        if error > 1e-7:
            raise ValueError("fixed whole-profile or point height mismatch")
        errors[contact["contact_id"]] = error
    return {"height_errors_m": errors, "samples": count}
