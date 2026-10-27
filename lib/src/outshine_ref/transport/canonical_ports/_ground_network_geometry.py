"""Shared junction cores and corridor bands for a small bound source tree."""

import time

import numpy as np
import shapely

from ._ground_domain import mesh_parts
from ._ground_recipe import NETWORK_RECIPE
from ._ground_star_geometry import profile_cuts


def node_sections(xy, edges, widths, seconds, cut_method):
    incidence = [[] for _ in xy]
    for edge, (a, b) in enumerate(edges):
        incidence[a].append((edge, b))
        incidence[b].append((edge, a))
    profiles, polygons, sources, node_parts, cuts_work = {}, [], [], {}, []
    for node, adjacent in enumerate(incidence):
        ends = np.array([xy[other] - xy[node] for _, other in adjacent])
        half = np.array([widths[edge] / 2 for edge, _ in adjacent])
        if len(adjacent) == 1:
            tangent = ends / np.linalg.norm(ends, axis=1)[:, None]
            normal = np.c_[-tangent[:, 1], tangent[:, 0]]
            cuts = np.zeros(1)
        else:
            attempted = [
                ("common" if len(adjacent) == 2 else "linear")
                if cut_method == "adaptive"
                else cut_method
            ]
            try:
                cuts, tangent, normal, work = profile_cuts(
                    ends,
                    half,
                    arm_count=len(adjacent),
                    seconds=seconds(),
                    method=attempted[0],
                )
            except ValueError as error:
                if cut_method != "adaptive" or str(error) not in (
                    "three_arm_cut_bounds",
                    "three_arm_common_cut_direction",
                ):
                    raise
                attempted.append("linear")
                cuts, tangent, normal, work = profile_cuts(
                    ends,
                    half,
                    arm_count=len(adjacent),
                    seconds=seconds(),
                    method="linear",
                )
            work.update(
                attempted_methods=attempted,
                processed_constraint_rows=len(attempted)
                * len(adjacent)
                * (len(adjacent) - 1),
            )
            cuts_work.append(work)
        sections = (
            xy[node]
            + cuts[:, None, None] * tangent[:, None]
            + np.array([-1, 1])[None, :, None] * half[:, None, None] * normal[:, None]
        )
        for (edge, _), section in zip(adjacent, sections, strict=True):
            profiles[node, edge] = section
        if len(adjacent) > 1:
            original = (
                xy[node]
                + np.array([-1, 1])[None, :, None]
                * half[:, None, None]
                * normal[:, None]
            )
            node_parts[node] = len(polygons)
            polygons.append(
                shapely.MultiPoint(
                    np.vstack((sections.reshape(-1, 2), original.reshape(-1, 2)))
                ).convex_hull
            )
            sources.append(tuple(edge for edge, _ in adjacent))
    return profiles, polygons, sources, node_parts, cuts_work


def edge_bands(a, b, width, start_leaf, end_leaf):
    length = float(np.linalg.norm(b.mean(axis=0) - a.mean(axis=0)))
    if length <= 0.1:
        raise ValueError("ground_tree_insufficient_corridor_length")
    start = min(2.0, 0.2 * length) if start_leaf else 0.0
    end = length - min(2.0, 0.2 * length) if end_leaf else length
    bands = [(0.0, start)] if start_leaf else []
    if length > 20.0:
        if not start_leaf and 1.2 * width < end - 0.05:
            bands.append((0.8 * width, 1.2 * width))
        if not end_leaf:
            candidate = (length - 1.2 * width, length - 0.8 * width)
            if candidate[0] > (bands[-1][1] if bands else start) + 0.05:
                bands.append(candidate)
    if end_leaf:
        bands.append((end, length))
    stations, roles = [0.0], []
    for low, high in bands:
        if low > stations[-1]:
            stations.append(low)
            roles.append(0)
        stations.append(high)
        roles.append(1)
    if stations[-1] < length:
        stations.append(length)
        roles.append(0)
    sections = [a + s / length * (b - a) for s in stations]
    sections[0], sections[-1] = a, b
    return sections, roles


def layout(xy, edges, widths, grades, *, seconds, cut_method="adaptive"):
    deadline = time.monotonic() + seconds
    remaining = lambda: deadline - time.monotonic()
    profiles, polygons, source_edges, node_parts, cut_work = node_sections(
        xy, edges, widths, remaining, cut_method
    )
    roles = [1] * len(polygons)
    frames = [np.eye(2) / min(0.025, min(grades[list(e)])) for e in source_edges]
    ports, links, caps, terminal_edges, terminal_nodes = [], [], [], [], []
    for edge, (a, b) in enumerate(edges):
        if remaining() <= 0:
            raise ValueError("ground_tree_geometry_deadline")
        tangent = (xy[b] - xy[a]) / np.linalg.norm(xy[b] - xy[a])
        normal = np.array([-tangent[1], tangent[0]])
        sections, local_roles = edge_bands(
            profiles[a, edge],
            profiles[b, edge][::-1],
            widths[edge],
            a not in node_parts,
            b not in node_parts,
        )
        first = len(polygons)
        for j, role in enumerate(local_roles):
            polygons.append(
                shapely.Polygon(np.vstack((sections[j], sections[j + 1][::-1])))
            )
            roles.append(role)
            source_edges.append((edge,))
            frames.append(np.stack((tangent / grades[edge], normal / 0.025)))
        for j, section in enumerate(sections[1:-1]):
            ports.append(section)
            links.append((first + j, first + j + 1))
        for node, section, part in (
            (a, sections[0], first),
            (b, sections[-1], len(polygons) - 1),
        ):
            if node in node_parts:
                ports.append(section)
                links.append((node_parts[node], part))
            else:
                caps.append(section)
                terminal_edges.append(edge)
                terminal_nodes.append(node)
    raw, report = mesh_parts(polygons, frames, roles, ports, links, caps)
    if remaining() <= 0:
        raise ValueError("ground_tree_geometry_deadline")
    report.update(
        recipe=NETWORK_RECIPE if cut_method == "adaptive" else None,
        part_source_edge_indices=source_edges,
        terminal_edge_indices=terminal_edges,
        terminal_node_indices=terminal_nodes,
        cut_fits=cut_work,
    )
    return raw, report
