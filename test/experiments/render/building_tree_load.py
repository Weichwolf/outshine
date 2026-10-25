"""Load only selected render arrays; keep source lineage out of parent reads."""

import numpy as np
from build_building_hierarchy import RENDER_GEOMETRY, RENDER_MATERIAL
from building_mass_appearance import policy_digest
from building_source_tree import select_cut, sources_at


def read_tree(root, *, lineage=True):
    with np.load(root / "tree.npz", allow_pickle=False) as data:
        keys = (
            data.files
            if lineage
            else (
                "node_bounds",
                "node_children",
                "basis",
                "anchor",
                "unbounded_sources",
                "tree_sha256",
            )
        )
        return {key: data[key] for key in keys}


def render_arrays(path):
    keys = ["g__" + k for k in RENDER_GEOMETRY] + ["m__" + k for k in RENDER_MATERIAL]
    with np.load(path, allow_pickle=False) as data:
        return {key: data[key] for key in keys}


def load_cut(root, report, tree, query, at, *, ready_arrays=None):
    """Optional caller-owned arrays belong to this immutable root/tree/recipe only."""
    ready_arrays = {} if ready_arrays is None else ready_arrays
    cut, selection = select_cut(tree, query["eye"], query["focal_px"])
    if cut.tolist() != query["nodes"]:
        raise ValueError("stored query and source hierarchy selection differ")
    needs = set(map(int, tree["unbounded_sources"]))
    positions, indices, owners, colours, weights, support = [], [], [], [], [], []
    coverage = []
    offset, group_count, read_bytes, files = 0, 0, 0, []
    loaded, reused = [], []
    source_map_bytes = 0
    palette = np.array(at["palette"])
    for node in cut:
        name = report["node_files"].get(str(int(node)))
        if name is None:
            if "source_order" not in tree:
                with np.load(root / "tree.npz", allow_pickle=False) as archive:
                    for key in ("source_order", "node_sources"):
                        tree[key] = archive[key]
                        source_map_bytes += tree[key].nbytes
            needs.update(map(int, sources_at(tree, node)))
            continue
        if name in ready_arrays:
            data = ready_arrays[name]
            reused.append(name)
        else:
            data = render_arrays(root / name)
            loaded.append(name)
            read_bytes += sum(v.nbytes for v in data.values())
        files.append(name)
        if not np.array_equal(data["g__tree_sha256"], tree["tree_sha256"]):
            raise ValueError("parent belongs to a different source hierarchy")
        if not np.array_equal(data["g__basis"], tree["basis"]) or not np.array_equal(
            data["g__anchor"], tree["anchor"]
        ):
            raise ValueError("parent and source hierarchy frames differ")
        if data["m__policy_sha256"].tobytes() != policy_digest(
            at
        ) or not np.array_equal(data["m__roughness_palette"], palette):
            raise ValueError("parent material recipe differs")
        ready_arrays[name] = data
        needs.update(map(int, data["g__needs_sources"]))
        positions.append(data["g__positions"])
        indices.append(data["g__indices"].astype(np.uint32) + offset)
        owners.append(data["g__triangle_groups"].astype(np.uint32) + group_count)
        colours.append(data["m__albedo"])
        coverage.append(data["g__group_coverage"])
        weights.append(data["m__roughness_weights"])
        support.append(data["m__supported"])
        offset += len(positions[-1])
        group_count += len(colours[-1])
    geometry = {
        "positions": np.vstack(positions)
        if positions
        else np.empty((0, 3), np.float32),
        "indices": np.vstack(indices) if indices else np.empty((0, 3), np.uint32),
        "triangle_groups": np.concatenate(owners) if owners else np.empty(0, np.uint32),
        "needs_sources": np.array(sorted(needs), np.uint32),
        "group_coverage": np.concatenate(coverage)
        if coverage
        else np.empty((0, 3), np.float32),
    }
    appearance = {
        "albedo": np.concatenate(colours)
        if colours
        else np.empty((0, 2, 3), np.float32),
        "roughness_weights": np.concatenate(weights)
        if weights
        else np.empty((0, 2, len(palette)), np.float32),
        "supported": np.concatenate(support) if support else np.empty(0, bool),
        "roughness_palette": palette,
    }
    return (
        geometry,
        appearance,
        {
            "parent_files": files,
            "new_parent_files": loaded,
            "reused_parent_files": reused,
            "parent_render_array_bytes_read": read_bytes,
            "full_tree_metadata_array_bytes": sum(v.nbytes for v in tree.values()),
            "parent_lineage_arrays_read": 0,
            "source_map_array_bytes_read_for_original_fallback": source_map_bytes,
            "selection": selection,
        },
    )
