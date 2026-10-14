"""Keep independently valid physical components of a bounded flat source plan."""

import itertools
import math
import time
from dataclasses import dataclass

import numpy as np
import shapely
from transport_flat_corners import rounded_plan
from transport_flat_surfaces import solve


@dataclass(frozen=True, slots=True)
class ComponentBudget:
    maximum_edges: int = 65536
    maximum_coordinates: int = 262144
    maximum_components: int = 2048
    maximum_component_edges: int = 16384
    maximum_faces: int = 262144
    seconds: float = 15.0

    def __post_init__(self):
        for name in (
            "maximum_edges",
            "maximum_coordinates",
            "maximum_components",
            "maximum_component_edges",
            "maximum_faces",
        ):
            value = getattr(self, name)
            if type(value) is not int or not 1 <= value <= 262144:
                raise ValueError("positive bounded component allocation required")
        if not math.isfinite(self.seconds) or self.seconds < 0:
            raise ValueError("finite nonnegative cooperative time required")


DEFAULT_BUDGET = ComponentBudget()


def component_domains(graphs, budget, precision, corner_radius, arc_error):
    sizes = [len(g["lines"]) for g in graphs]
    if not 1 <= sum(sizes) <= budget.maximum_edges:
        raise ValueError("component_input_edge_budget")
    lines = np.concatenate([g["lines"] for g in graphs])
    half = np.concatenate([g["half_widths"] for g in graphs])
    bands = shapely.buffer(
        lines, half, quad_segs=2, cap_style="round", join_style="round"
    )
    complete = shapely.set_precision(
        shapely.set_precision(shapely.union_all(bands), precision), 0
    )
    if corner_radius > 0:
        complete, _ = rounded_plan(complete, corner_radius, arc_error)
    domains = shapely.get_parts(complete)
    if not np.all(shapely.get_type_id(domains) == 3):
        raise ValueError("nonpolygonal_component_domain")
    points = shapely.point_on_surface(bands)
    hits = shapely.STRtree(domains).query(points, predicate="within")
    counts = np.bincount(hits[0], minlength=len(lines))
    if np.any(counts != 1):
        raise ValueError("component_edge_without_unique_domain")
    labels = np.empty(len(lines), dtype=int)
    labels[hits[0]] = hits[1]
    return domains, labels, np.r_[0, np.cumsum(sizes)]


def select_graph(graph, indices):
    nodes, edges = np.unique(graph["edges"][indices], return_inverse=True)
    selected = {
        "vertices": graph["vertices"][nodes],
        "edges": edges.reshape(-1, 2),
        "lines": graph["lines"][indices],
        "half_widths": graph["half_widths"][indices],
    }
    for key in ("travel_public", "travel_restricted"):
        if key in graph:
            selected[key] = graph[key][indices].copy()
    return selected


def selected_sources(graphs, selections):
    owners = []
    for graph, indices in zip(graphs, selections, strict=True):
        if "owner_offsets" not in graph:
            continue
        cuts = graph["owner_offsets"]
        for index in indices:
            owners.extend(graph["owner_sources"][cuts[index] : cuts[index + 1]])
    return sorted(set(map(int, owners)))


def solve_component(selected, domain, budget, precision, radius, error):
    product, audit = solve(
        selected, precision, junction_mode="cuts", corner_radius=radius, arc_error=error
    )
    if len(product["indices"]) > budget.maximum_faces:
        raise ValueError("component_face_budget")
    if any(
        (
            audit["ports"]["invalid_sections"],
            audit["cuts"]["unresolved_cuts"],
            audit["cuts"]["remaining_cut_conflicts"],
        )
    ):
        raise ValueError("unresolved_component_ports_or_cuts")
    coverage = shapely.union_all(
        shapely.polygons(product["vertices"][product["indices"]])
    )
    difference = float(domain.symmetric_difference(coverage).area)
    if difference > max(1e-7, domain.area * 1e-12):
        raise ValueError("component_mesh_changed_physical_domain")
    audit["component_domain_difference_m2"] = difference
    return product, audit


def global_product(graphs, budget, precision, radius, error):
    product, audit = solve(
        graphs, precision, junction_mode="cuts", corner_radius=radius, arc_error=error
    )
    if any(
        (
            audit["ports"]["invalid_sections"],
            audit["cuts"]["unresolved_cuts"],
            audit["cuts"]["remaining_cut_conflicts"],
        )
    ):
        raise ValueError("unresolved_global_ports_or_cuts")
    if len(product["indices"]) > budget.maximum_faces:
        raise ValueError("global_output_face_budget")
    return product, audit


def fast_record(graphs, product, audit):
    selections = [np.arange(len(g["lines"])) for g in graphs]
    return {
        "component": "global_validated_mesh",
        "source_indices": selected_sources(graphs, selections),
        "original_edges": [
            [i, int(edge)] for i, indices in enumerate(selections) for edge in indices
        ],
        "edges": sum(map(len, selections)),
        "area_m2": audit["area_m2"],
        "valid": True,
        "reason": "",
        "audit": audit,
        "product": 0,
    }


def input_limit(graphs, budget):
    edges = sum(len(g["lines"]) for g in graphs)
    if not 1 <= edges <= budget.maximum_edges:
        return "component_input_edge_budget"
    coordinates = sum(
        int(shapely.get_num_coordinates(g["lines"]).sum()) for g in graphs
    )
    return (
        "component_input_coordinate_budget"
        if coordinates > budget.maximum_coordinates
        else ""
    )


def local_products(
    graphs, domains, labels, offsets, budget, precision, corner_radius, arc_error, began
):
    records, products = [], []
    # Small independent parts survive before a large, potentially difficult part.
    order = sorted(range(len(domains)), key=lambda i: (domains[i].area, i))
    output_faces = 0
    for ordinal, component in enumerate(order):
        selections = [
            np.flatnonzero(labels[a:b] == component)
            for a, b in itertools.pairwise(offsets)
        ]
        edges = sum(map(len, selections))
        record = {
            "component": component,
            "source_indices": selected_sources(graphs, selections),
            "original_edges": [
                [i, int(edge)]
                for i, indices in enumerate(selections)
                for edge in indices
            ],
            "edges": edges,
            "area_m2": float(domains[component].area),
            "valid": False,
            "reason": "",
        }
        records.append(record)
        if ordinal >= budget.maximum_components:
            record["reason"] = "component_count_budget"
            continue
        if edges > budget.maximum_component_edges:
            record["reason"] = "component_edge_budget"
            continue
        if time.monotonic() - began >= budget.seconds:
            record["reason"] = "component_time_budget"
            continue
        selected = [
            select_graph(g, indices)
            for g, indices in zip(graphs, selections, strict=True)
            if len(indices)
        ]
        try:
            product, audit = solve_component(
                selected,
                domains[component],
                budget,
                precision,
                corner_radius,
                arc_error,
            )
        except (ValueError, shapely.errors.GEOSException) as failure:
            record["reason"] = str(failure)
            continue
        if output_faces + len(product["indices"]) > budget.maximum_faces:
            record["reason"] = "component_total_face_budget"
            continue
        record.update(valid=True, audit=audit, product=len(products))
        products.append(product)
        output_faces += len(product["indices"])
    return products, records


def assemble(
    graphs,
    *,
    budget=DEFAULT_BUDGET,
    precision=0.001,
    corner_radius=2.0,
    arc_error=0.025,
    prefer_global=True,
):
    """Return valid complete components, explicit local failures and original owners.

    Components follow the physical band union after curb refinement, rather than
    disconnected axis graphs. Whole preprocessing and each geometry call remain
    cooperative; hard time/address-space limits belong to the isolated caller.
    This is a flat geometry product, with no implied drivable route or ground datum.
    """
    began = time.monotonic()
    if not isinstance(budget, ComponentBudget):
        raise TypeError("explicit ComponentBudget required")
    if not all(math.isfinite(v) and v > 0 for v in (precision, arc_error)):
        raise ValueError("positive finite precision and arc error required")
    if not math.isfinite(corner_radius) or corner_radius < 0:
        raise ValueError("finite nonnegative curb radius required")
    records, products = [], []
    report = {
        "complete": False,
        "valid_components": 0,
        "components": records,
        "scope": "Flat physical components; source/frame, levels and route usability caller-owned.",
        "hard_wall_time_bound": False,
    }
    if time.monotonic() - began >= budget.seconds:
        report["preparation_reason"] = "component_time_budget"
        return products, report
    reason = input_limit(graphs, budget)
    if reason:
        report["preparation_reason"] = reason
        return products, report
    if prefer_global:
        try:
            product, audit = global_product(
                graphs, budget, precision, corner_radius, arc_error
            )
        except (ValueError, shapely.errors.GEOSException) as failure:
            report["global_failure"] = str(failure)
        else:
            records.append(fast_record(graphs, product, audit))
            report.update(
                complete=True,
                valid_components=1,
                fast_path=True,
                elapsed_ms=(time.monotonic() - began) * 1000,
            )
            return [product], report
    try:
        domains, labels, offsets = component_domains(
            graphs, budget, precision, corner_radius, arc_error
        )
    except (ValueError, shapely.errors.GEOSException) as failure:
        report["preparation_reason"] = str(failure)
        return products, report
    products, local = local_products(
        graphs,
        domains,
        labels,
        offsets,
        budget,
        precision,
        corner_radius,
        arc_error,
        began,
    )
    records.extend(local)
    report.update(
        complete=bool(records) and all(r["valid"] for r in records),
        valid_components=len(products),
        elapsed_ms=(time.monotonic() - began) * 1000,
    )
    return products, report
