"""Build only demanded source-tree nodes; no original fine mesh is an input."""

from collections import defaultdict

import numpy as np
import shapely
from building_coverage_field import source_projections
from building_mass_inputs import describe
from building_mass_mesh import extrude
from building_parent_buffer import expand
from building_source_tree import sources_at


def prepare_sources(parent, cells, requested, frame):
    masses, forms, needs = {}, {}, []
    read, described = 0, 0
    if not requested:
        return (
            masses,
            forms,
            {
                "decoded_sources": 0,
                "described_sources": 0,
                "sources_without_contour_work": 0,
                "source_needs": [],
                "form_reads": 0,
                "form_bytes": 0,
            },
        )
    seen = set()
    before_reads, before_bytes = parent.form_reads, parent.form_bytes
    for _, identities, surfaces in parent.iter_forms(cells):
        for identity, surface in zip(identities, surfaces, strict=True):
            identity = int(identity)
            seen.add(identity)
            read += 1
            if identity not in requested:
                continue
            described += 1
            try:
                mass, _ = describe(identity, surface, frame["basis"], frame["anchor"])
                masses[identity], forms[identity] = mass, surface
            except (ValueError, ArithmeticError, shapely.errors.GEOSException) as error:
                needs.append({"source": identity, "reason": str(error)})
    if not set(requested) <= seen:
        raise ValueError("requested source forms must belong to supplied cells")
    return (
        masses,
        forms,
        {
            "decoded_sources": read,
            "described_sources": described,
            "sources_without_contour_work": read - described,
            "source_needs": needs,
            "form_reads": parent.form_reads - before_reads,
            "form_bytes": parent.form_bytes - before_bytes,
        },
    )


def projected_mass_coverage(group, domain, heights):
    source_areas = [p.area for p in source_projections(group)]
    proxy_areas = [domain.area]
    for axis in (0, 1):
        proxy = shapely.union_all(
            [
                shapely.box(p.bounds[axis], heights[0], p.bounds[axis + 2], heights[1])
                for p in shapely.get_parts(domain)
            ]
        )
        proxy_areas.append(proxy.area)
    ratios = np.array(source_areas) / proxy_areas
    return np.minimum(ratios, 1).tolist(), ratios.tolist()


def band_mesh(group, scale, *, buffer_path="scalar"):
    original = shapely.union_all([s.footprint for s in group])
    holes = shapely.union_all(
        [shapely.Polygon(r) for p in shapely.get_parts(original) for r in p.interiors]
    )
    holes = shapely.difference(holes, original)
    expanded = expand(original, scale, buffer_path)
    closed = shapely.buffer(expanded, -scale, join_style="mitre", mitre_limit=2)
    domain = shapely.difference(
        shapely.simplify(closed, scale / 2, preserve_topology=True), holes
    )
    if (
        not domain.is_valid
        or domain.is_empty
        or shapely.intersection(domain, holes).area > 0
    ):
        raise ValueError("valid nonempty parent with original holes required")
    heights = [min(s.bottom[0] for s in group), max(s.roof[1] for s in group)]
    mesh, work = extrude(domain, heights)
    opacity, ratios = projected_mass_coverage(group, domain, heights)
    return (
        mesh,
        heights,
        work
        | {
            "source_coordinates": int(shapely.get_num_coordinates(original)),
            "proxy_coordinates": int(shapely.get_num_coordinates(domain)),
            "added_plan_area_m2": float(shapely.difference(domain, original).area),
            "removed_plan_area_m2": float(shapely.difference(original, domain).area),
            "protected_hole_area_m2": float(holes.area),
            "projection_coverage": opacity,
            "unclipped_projection_area_ratios": ratios,
        },
    )


def node_bands(tree, node, masses):
    ids = sources_at(tree, node)
    low, high = tree["node_bounds"][node]
    scale = float(np.linalg.norm(high - low) / 32)
    bands = defaultdict(list)
    if len(ids) < 2 or scale <= 0:
        return bands, scale
    for identity in ids:
        source = masses.get(int(identity))
        if (
            source is None
            or source.raised
            or source.roof[1] - source.roof[0] > scale
            or source.bottom[1] - source.bottom[0] > scale
        ):
            continue
        key = (
            int(np.floor((source.bottom[0] - low[2]) / scale)),
            int(np.floor((source.roof[1] - low[2]) / scale)),
        )
        bands[key].append(source)
    return bands, scale


def build_node(tree, node, masses, *, buffer_path="scalar"):
    groups, failures = [], []
    bands, scale = node_bands(tree, node, masses)
    for group in bands.values():
        if len(group) < 2:
            continue
        ids = [s.source for s in group]
        try:
            mesh, interval, work = band_mesh(group, scale, buffer_path=buffer_path)
        except (ValueError, ArithmeticError, shapely.errors.GEOSException) as error:
            failures.append({"node": node, "sources": ids, "reason": str(error)})
            continue
        groups.append(
            (
                mesh,
                interval,
                {
                    "node": node,
                    "sources": ids,
                    "closing_radius_m": scale,
                    "simplification_m": scale / 2,
                    **work,
                },
            )
        )
    return groups, failures


def build_cut(tree, cut, masses, frame, *, ready_nodes=None):
    ready_nodes = {} if ready_nodes is None else ready_nodes
    covered = (
        np.concatenate([sources_at(tree, node) for node in cut])
        if len(cut)
        else np.empty(0, np.uint32)
    )
    if (
        len(np.unique(covered)) != len(covered)
        or not np.isin(covered, frame["selected_sources"]).all()
    ):
        raise ValueError("cut must select nonoverlapping whole sources from this frame")
    positions, indices, owners, sources, offsets, heights, nodes = (
        [],
        [],
        [],
        [],
        [0],
        [],
        [],
    )
    reports, failures, count, generated = [], [], 0, 0
    coverage = []
    for node in cut:
        node = int(node)
        if node not in ready_nodes:
            ready_nodes[node] = build_node(tree, node, masses)
            generated += 1
        groups, failed = ready_nodes[node]
        failures.extend(failed)
        for mesh, interval, work in groups:
            ids = work["sources"]
            if (
                count + len(mesh["positions"]) > 65536
                or len(owners) + len(mesh["indices"]) > 131072
            ):
                failures.append(
                    {
                        "node": node,
                        "sources": ids,
                        "reason": "complete diagnostic cut geometry budget exhausted",
                    }
                )
                continue
            positions.append(mesh["positions"])
            indices.append(mesh["indices"].astype(np.uint32) + count)
            count += len(mesh["positions"])
            owners.extend([len(reports)] * len(mesh["indices"]))
            sources.extend(ids)
            offsets.append(len(sources))
            heights.append(interval)
            nodes.append(node)
            reports.append(work)
            coverage.append(work["projection_coverage"])
    merged = np.array(sources, np.uint32)
    rest = np.setdiff1d(frame["selected_sources"], merged)
    if len(np.unique(merged)) != len(merged) or not np.array_equal(
        np.sort(np.r_[merged, rest]), np.sort(frame["selected_sources"])
    ):
        raise ValueError(
            "each whole original source must belong to exactly one product"
        )
    product = {k: frame[k] for k in ("basis", "anchor", "selected_sources")}
    if "tree_sha256" in frame:
        product["tree_sha256"] = frame["tree_sha256"]
    product.update(
        {
            "positions": np.vstack(positions)
            if positions
            else np.empty((0, 3), np.float32),
            "indices": np.vstack(indices).astype(np.uint16)
            if indices
            else np.empty((0, 3), np.uint16),
            "triangle_groups": np.array(owners, np.uint16),
            "group_sources": merged,
            "group_offsets": np.array(offsets, np.uint32),
            "group_heights": np.array(heights).reshape(-1, 2),
            "group_nodes": np.array(nodes, np.uint32),
            "group_coverage": np.array(coverage, np.float32).reshape(-1, 3),
            "needs_sources": rest,
        }
    )
    return product, {
        "groups": reports,
        "declined_groups": failures,
        "built_groups": len(reports),
        "selected_nodes": len(cut),
        "original_needs": len(rest),
        "new_node_products": generated,
        "reused_node_products": len(cut) - generated,
        "diagnostic_assembly_array_bytes": sum(v.nbytes for v in product.values()),
        "array_bytes": {k: v.nbytes for k, v in product.items()},
        "scope": "Camera-independent node/height-band candidates from actual BSC3 footprints. Bounds only selected demand. Closing and topology-preserving simplification are measured approximations, not surface/coverage/contact/transport/LOD/native admission. Whole original remainder paid; no fine meshes constructed by this producer.",
    }
