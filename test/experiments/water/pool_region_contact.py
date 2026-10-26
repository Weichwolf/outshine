"""Bounded original-grid patches exchange exact knots before joining a window."""

from collections import defaultdict
from dataclasses import dataclass, field
from fractions import Fraction

import numpy as np
import shapely
from conforming_body_contact import contact_error
from outshine_ref.contracts import Mesh
from outshine_ref.geometry.sampling import SurfaceSampler
from pool_contact_neighbors import boundary_edges, edge_profile, profile_gap
from pool_terrain_contact import ContactSurface, contact_patch
from terrain_grid_lod import triangles


@dataclass
class Patch:
    source_ids: np.ndarray
    source: np.ndarray
    faces: np.ndarray
    result: tuple | None = None
    requests: list = field(default_factory=list)

    def profile(self, edge):
        if self.result is None:
            return {
                Fraction(0): self.source[edge[0]],
                Fraction(1): self.source[edge[1]],
            }
        return edge_profile(self.source, self.result, edge)

    def mesh(self):
        return self.result[:2] if self.result is not None else (self.source, self.faces)


def _patches(body, vertices, side, apron_m):
    if not 5 <= side <= 129 or (side - 1) % 4 or len(vertices) != side * side:
        raise ValueError(
            "bounded square native grid divisible into four-cell patches required"
        )
    ids = np.arange(len(vertices)).reshape(side, side)
    outer = shapely.polygons(body["plan_xy"][body["outer_ring"]])
    result, owners = [], defaultdict(list)
    for row in range(0, side - 1, 4):
        for column in range(0, side - 1, 4):
            source_ids = ids[row : row + 5, column : column + 5].ravel()
            source = vertices[source_ids]
            patch = Patch(source_ids, source, triangles(5, 1).astype(np.int64))
            low, high = source[:, :2].min(axis=0), source[:, :2].max(axis=0)
            selected = shapely.distance(shapely.box(*low, *high), outer) < apron_m
            result.append((patch, bool(selected)))
            for a, b in boundary_edges(patch.faces):
                edge = int(source_ids[a]), int(source_ids[b])
                owners[edge].append((patch, (int(a), int(b))))
    return result, owners


def _exchange(owners):
    gap, checked = 0.0, 0
    for users in owners.values():
        if len(users) != 2:
            continue
        (a, ae), (b, be) = users
        if a.result is None and b.result is None:
            continue
        ap, bp = a.profile(ae), b.profile(be)
        gap = max(gap, profile_gap(ap, bp))
        checked += 1
        union = ap.keys() | bp.keys()
        for patch, edge, profile in ((a, ae, ap), (b, be, bp)):
            patch.requests.extend(
                {"input_edge": list(edge), "parameter": str(t)}
                for t in sorted(union - profile.keys())
                if 0 < t < 1
            )
    return gap, checked


def _seams(owners):
    gap, outer_changed = 0.0, 0
    for users in owners.values():
        if len(users) == 1:
            patch, edge = users[0]
            profile = patch.profile(edge)
            changed = len(profile) > 2 or any(
                value.tobytes() != patch.source[edge[int(t)]].tobytes()
                for t, value in profile.items()
                if t in (0, 1)
            )
            outer_changed += int(changed)
            continue
        (a, ae), (b, be) = users
        if a.result is None and b.result is None:
            continue
        ap, bp = a.profile(ae), b.profile(be)
        if ap.keys() != bp.keys():
            raise ValueError("window shared-edge subdivision remains unresolved")
        gap = max(gap, profile_gap(ap, bp))
        for t in ap:
            if (
                ap[t][:2].tobytes() != bp[t][:2].tobytes()
                or ap[t].astype(np.float32).tobytes()
                != bp[t].astype(np.float32).tobytes()
            ):
                raise ValueError("window shared-edge position words disagree")
    if gap > 1e-7:
        raise ValueError("window shared-edge height agreement failed")
    return gap, outer_changed


def _join(patches):
    arrays, indices, offset = [], [], 0
    for patch, _ in patches:
        vertices, faces = patch.mesh()
        arrays.append(vertices)
        indices.append(faces + offset)
        offset += len(vertices)
    source = np.vstack(arrays)
    _, first, inverse = np.unique(
        source[:, :2], axis=0, return_index=True, return_inverse=True
    )
    vertices = source[first]
    if np.any(abs(source[:, 2] - vertices[inverse, 2]) > 1e-7):
        raise ValueError("window weld would hide differing heights")
    if (
        source.astype(np.float32).tobytes()
        != vertices[inverse].astype(np.float32).tobytes()
    ):
        raise ValueError("window weld would hide differing Float32 words")
    return (
        vertices,
        inverse[np.vstack(indices)],
        sum(v.nbytes + f.nbytes for v, f in (patch.mesh() for patch, _ in patches)),
    )


def _audit(body, original, vertices, faces, side):
    edges = np.sort(faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1)
    edges, counts = np.unique(edges, axis=0, return_counts=True)
    cross = np.cross(
        vertices[faces[:, 1]] - vertices[faces[:, 0]],
        vertices[faces[:, 2]] - vertices[faces[:, 0]],
    )[:, 2]
    euler = len(vertices) - len(edges) + len(faces)
    if np.any(cross <= 0) or np.any((counts != 1) & (counts != 2)) or euler != 1:
        raise ValueError("window candidate is not a positive indexed disk")
    lookup = {tuple(v[:2]): i for i, v in enumerate(vertices)}
    if not all(tuple(v[:2]) in lookup for v in original):
        raise ValueError("window lost original source positions")
    original_indices = np.array(
        [lookup[tuple(v[:2])] for v in original], dtype=np.int64
    )
    model = ContactSurface(
        SurfaceSampler(Mesh(body["contact_vertices"], body["contact_faces"]))
    )
    errors = contact_error(model, vertices, faces)
    ids = np.arange(len(original)).reshape(side, side)
    ring = np.concatenate((ids[0], ids[1:, -1], ids[-1, -2::-1], ids[-2:0:-1, 0]))
    domain = shapely.polygons(original[ring, :2])
    complete = bool(
        shapely.covers(domain, shapely.coverage_union_all(model.sampler._polygons))
    )
    if (
        max(
            errors["maximum_above_underside_m"], errors["maximum_gap_below_underside_m"]
        )
        > 1e-7
    ):
        raise ValueError("window failed its whole contact comparison")
    return (
        original_indices,
        errors,
        {
            "euler": euler,
            "boundary_edges": int((counts == 1).sum()),
            "interior_edges": int((counts == 2).sum()),
            "complete_body_contact_covered": complete,
        },
    )


def reconcile_window(
    body,
    vertices,
    side,
    *,
    apron_m=3.0,
    reuse_unchanged=True,
    cancelled=None,
    progress=None,
):
    """Close shared edges in one supplied frame; keep outer obligations explicit."""
    if cancelled is not None and cancelled():
        raise ValueError("pool window contact cancelled")
    if vertices.ndim != 2 or vertices.shape[1] != 3 or not np.isfinite(vertices).all():
        raise ValueError("finite explicit window positions required")
    if not np.isfinite(apron_m) or not 0 < apron_m <= 10:
        raise ValueError("bounded explicit window apron required")
    patches, owners = _patches(body, vertices, side, apron_m)
    work = defaultdict(int)

    def build(patch, requests=()):
        if cancelled is not None and cancelled():
            raise ValueError("pool window contact cancelled")
        result = contact_patch(
            body, patch.source, patch.faces, apron_m=apron_m, boundary_requests=requests
        )
        for key in (
            "input_triangles",
            "candidate_pairs",
            "pieces",
            "edge_node_tests",
            "edge_node_determinants",
            "edge_node_interval_visits",
            "edge_node_binary_searches",
            "edge_node_sorted_references",
            "boundary_requested_nodes",
        ):
            work[key] += result[3]["work"][key]
        work["patch_calls"] += 1
        work["returned_numeric_bytes"] += sum(a.nbytes for a in result[:3])
        return result

    for index, (patch, selected) in enumerate(patches):
        if cancelled is not None and cancelled():
            raise ValueError("pool window contact cancelled")
        if selected:
            patch.result = build(patch)
        if progress is not None and index % 32 == 0:
            progress("partition", index, len(patches))
    before, checked = _exchange(owners)
    rebuilds = 0
    for index, (patch, _) in enumerate(patches):
        if patch.requests or (patch.result is not None and not reuse_unchanged):
            patch.result = build(patch, patch.requests)
            rebuilds += 1
        if progress is not None and index % 32 == 0:
            progress("sew", index, len(patches))
    after, outer = _seams(owners)
    result, faces, patch_bytes = _join(patches)
    original_indices, errors, topology = _audit(body, vertices, result, faces, side)
    return (
        result,
        faces,
        original_indices,
        {
            "patches": len(patches),
            "initial_contact_patches": sum(s for _, s in patches),
            "rebuilt_patches": rebuilds,
            "untouched_patches": sum(p.result is None for p, _ in patches),
            "shared_original_edges_checked": checked,
            "maximum_initial_seam_gap_m": before,
            "maximum_reconciled_seam_gap_m": after,
            "outer_changed_original_edges": outer,
            "contact": errors,
            "topology": topology,
            "work": dict(work),
            "patch_mesh_bytes_before_join": patch_bytes,
            "admission": (
                "partial_body_window"
                if not topology["complete_body_contact_covered"]
                else "reference_window_closed"
                if outer == 0
                else "pending_outer_neighbors"
            ),
            "scope": "Bounded original window in one WGS84 Double reference; native page/frame packing, other world contacts and device budgets remain separate.",
        },
    )
