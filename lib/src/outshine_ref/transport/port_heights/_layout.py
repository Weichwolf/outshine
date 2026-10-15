"""Assign shared junction planes and full-width transition patches to existing modules."""

import itertools

import numpy as np

from ._field import PlaneField


class Groups:
    def __init__(self, count):
        self.parent = np.arange(count)

    def root(self, item):
        while self.parent[item] != item:
            self.parent[item] = self.parent[self.parent[item]]
            item = self.parent[item]
        return int(item)

    def merge(self, items):
        roots = [self.root(int(i)) for i in items]
        for root in roots:
            self.parent[root] = min(roots)


def part_vertices(faces, owners, count):
    order = np.argsort(owners, kind="stable")
    offsets = np.r_[0, np.cumsum(np.bincount(owners, minlength=count))]
    return [np.unique(faces[order[a:b]]) for a, b in itertools.pairwise(offsets)]


def oriented_ports(product):
    edges, adjacent = product["port_edges"], product["port_parts"]
    roles, owners = product["part_roles"], product["face_owners"]
    if np.any(roles[adjacent].sum(axis=1) != 1):
        raise ValueError("every existing port must join a corridor and junction")
    road = np.where(roles[adjacent[:, 0]] == 0, adjacent[:, 0], adjacent[:, 1])
    core = np.where(roles[adjacent[:, 0]] == 1, adjacent[:, 0], adjacent[:, 1])
    wanted = {tuple(sorted(edge)): [] for edge in edges}
    if len(wanted) != len(edges):
        raise ValueError(
            "duplicate physical port edges require connected-section planning"
        )
    for face, corners in enumerate(product["indices"]):
        for a, b in ((0, 1), (1, 2), (2, 0)):
            key = tuple(sorted((corners[a], corners[b])))
            if key in wanted:
                wanted[key].append(face)
    xy = product["vertices"]
    ends = xy[edges]
    centers = ends.mean(axis=1)
    tangent = ends[:, 1] - ends[:, 0]
    width = np.linalg.norm(tangent, axis=1)
    if np.any(width <= 0):
        raise ValueError("positive whole port widths required")
    normals = np.c_[-tangent[:, 1], tangent[:, 0]] / width[:, None]
    for index, edge in enumerate(edges):
        touching = wanted[tuple(sorted(edge))]
        inside = [f for f in touching if owners[f] == road[index]]
        if len(touching) != 2 or len(inside) != 1:
            raise ValueError(
                "each full port must have one face from each original module"
            )
        if not np.array_equal(np.sort(owners[touching]), np.sort(adjacent[index])):
            raise ValueError(
                "whole port module IDs must match their actual adjacent faces"
            )
        delta = xy[product["indices"][inside[0]]].mean(axis=0) - centers[index]
        sign = np.dot(delta, normals[index])
        if abs(sign) <= 1e-12:
            raise ValueError("degenerate corridor face at a full port")
        normals[index] *= np.sign(sign)
    return road, core, centers, normals, width


def patch_geometry(points, centers, normals):
    scale = max(1.0, float(np.linalg.norm(np.ptp(points, axis=0))))
    signed = np.einsum("nki,ki->nk", points[:, None] - centers, normals) / scale
    d = signed.sum(axis=1)
    if d.min() <= 1e-4:
        return scale, "nonpositive_or_small_blend_denominator"
    if d.max() / d.min() > 32:
        return scale, "blend_denominator_condition_budget"
    if abs(signed[:, 0] / d).max() > 4:
        return scale, "blend_parameter_excursion_budget"
    return scale, ""


def prepare_plane_field(product):
    xy, faces = product["vertices"], product["indices"]
    roles, owners = product["part_roles"], product["face_owners"]
    if (
        len(xy) > 65536
        or len(faces) > 65536
        or len(roles) > 8192
        or len(product["port_edges"]) > 16384
    ):
        raise ValueError("explicit plane-layout input budget exceeded")
    vertices = part_vertices(faces, owners, len(roles))
    if any(not len(v) for v in vertices):
        raise ValueError("every retained original module must have faces")
    road, core, centers, normals, widths = oriented_ports(product)
    ports = [[] for _ in roles]
    for index, part in enumerate(road):
        ports[part].append(index)
    groups, fallback = Groups(len(roles)), []
    cuts = np.zeros((len(roles), 2, 2))
    directions = np.zeros_like(cuts)
    scales = np.ones(len(roles))
    for part in np.flatnonzero(roles == 0):
        selected = ports[part]
        if len(selected) > 2:
            groups.merge(core[selected])
            fallback.append(
                {
                    "part": int(part),
                    "reason": "multiport_corridor_shared_plane",
                    "port_ids": selected,
                }
            )
        elif len(selected) == 2:
            cuts[part], directions[part] = centers[selected], normals[selected]
            scales[part], reason = patch_geometry(
                xy[vertices[part]], cuts[part], directions[part]
            )
            if reason:
                groups.merge(core[selected])
                fallback.append(
                    {"part": int(part), "reason": reason, "port_ids": selected}
                )
    roots = sorted({groups.root(int(i)) for i in np.flatnonzero(roles == 1)})
    node_of = {root: i for i, root in enumerate(roots)}
    members = {root: [] for root in roots}
    for part in np.flatnonzero(roles == 1):
        members[groups.root(int(part))].append(xy[vertices[part]])
    origins = [np.vstack(members[root]).mean(axis=0) for root in roots]
    parents = np.full((len(roles), 2), -1, np.int64)
    for part in np.flatnonzero(roles == 1):
        parents[part] = node_of[groups.root(int(part))]
    for part in np.flatnonzero(roles == 0):
        selected = ports[part]
        if not selected:
            parents[part] = len(origins)
            origins.append(xy[vertices[part]].mean(axis=0))
        elif len(selected) != 2:
            parents[part] = node_of[groups.root(int(core[selected[0]]))]
            if len(selected) == 1:
                cuts[part, 0], directions[part, 0] = (
                    centers[selected[0]],
                    normals[selected[0]],
                )
                reach = float(
                    ((xy[vertices[part]] - cuts[part, 0]) @ directions[part, 0]).max()
                )
                if reach > 0.5:
                    cuts[part, 1] = cuts[part, 0] + directions[part, 0] * reach
                    directions[part, 1] = -directions[part, 0]
                    scales[part] = reach
                    parents[part, 1] = len(origins)
                    origins.append(cuts[part, 1].copy())
                else:
                    fallback.append(
                        {
                            "part": int(part),
                            "reason": "short_terminal_shared_plane",
                            "port_ids": selected,
                        }
                    )
        else:
            parents[part] = [node_of[groups.root(int(core[p]))] for p in selected]
    field = PlaneField(origins, parents, cuts, directions, scales)
    report = {
        "parts": len(roles),
        "original_ports": len(road),
        "plane_nodes": len(origins),
        "original_junction_parts": int(roles.sum()),
        "shared_junction_plane_groups": len(roots),
        "blended_parts": int(np.count_nonzero(parents[:, 0] != parents[:, 1])),
        "explicit_local_plane_recipes": fallback,
        "source_xy_and_face_ownership_unchanged": True,
        "port_widths_m": widths.tolist(),
    }
    return field, report
