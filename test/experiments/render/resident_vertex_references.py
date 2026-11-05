"""Measure cluster-local vertex addressing from an explicit native resident snapshot."""

import argparse
import json
from pathlib import Path

import numpy as np


def load(directory):
    metadata = json.loads((directory / "geometry.json").read_text())
    if metadata["version"] != 1 or not metadata["selection_ready"]:
        raise ValueError("version 1 with an accepted GPU selection required")
    order = "<" if metadata["byte_order"] == "little" else ">"
    streams = {}
    for name, spec in metadata["streams"].items():
        count = spec["rows"] * spec["columns"]
        path = directory / spec["file"]
        if spec["bytes"] != count * 4 or path.stat().st_size != count * 4:
            raise ValueError(f"invalid stream size: {name}")
        streams[name] = np.fromfile(path, dtype=order + "u4").reshape(
            spec["rows"], spec["columns"]
        )
    return metadata, streams


def measure(metadata, streams):
    roots, templates, batches = (streams[key] for key in ("roots", "templates", "batches"))
    source, kept, visible = (
        streams[key].reshape(-1) for key in ("source_indices", "kept", "visible")
    )
    arguments = streams["arguments"].reshape(-1, 5)
    if len(roots) != len(visible) or len(arguments) != len(batches):
        raise ValueError("root visibility or batch argument dimensions disagree")
    selected = np.zeros(len(batches), dtype=np.uint64)
    unique_ranges = {}
    source_spans = {}
    selected_vertex_references = 0
    selected_clusters = 0
    all_clusters = 0
    states = {}
    for root_index, root in enumerate(roots):
        first, count, logical, batch, _, base, _, flags = map(int, root[4:])
        if first + count > len(templates) or logical + count > len(kept) or batch >= len(batches):
            raise ValueError("root exceeds template, logical or batch addressing")
        key = tuple(map(int, batches[batch, [0, 1, 2, 3, 4, 5, 11]]))
        state = states.setdefault(key, {"batches": set(), "roots": 0,
                                        "visible_main_roots": 0,
                                        "root_input_indices": 0,
                                        "main_selected_indices": 0})
        state["batches"].add(batch)
        state["roots"] += 1
        main_visible = bool(visible[root_index]) and not flags & 4
        state["visible_main_roots"] += main_visible
        for local, item in enumerate(templates[first:first + count]):
            start, indices = base + int(item[1]), int(item[2])
            if start + indices > len(source) or indices % 3:
                raise ValueError("template exceeds source index addressing or splits a triangle")
            key = (start, indices)
            if key not in unique_ranges:
                references = source[start:start + indices]
                unique_ranges[key] = len(np.unique(references))
                source_spans[key] = int(references.max()) - int(references.min()) + 1 if indices else 0
            all_clusters += 1
            state["root_input_indices"] += indices
            if not main_visible:
                continue
            value = int(kept[logical + local])
            if value not in (0, int(item[2])):
                raise ValueError("selected cluster count disagrees with its template")
            selected[batch] += value
            state["main_selected_indices"] += value
            selected_clusters += value > 0
            selected_vertex_references += unique_ranges[key] if value else 0
    clustered = batches[:, 10] > 0
    if not np.array_equal(selected[clustered], arguments[clustered, 0]):
        raise ValueError("reconstructed selected indices disagree with native draw arguments")
    unique_counts = np.array(list(unique_ranges.values()), dtype=np.uint64)
    maximum = int(unique_counts.max()) if len(unique_counts) else 0
    shift = max(0, maximum - 1).bit_length()
    index_count = sum(count for _, count in unique_ranges)
    reference_count = int(unique_counts.sum())
    local_bytes = 1 if maximum <= 256 else 2 if maximum <= 65536 else 4
    span_counts = np.array(list(source_spans.values()), dtype=np.uint64)
    delta_bytes = sum(4 + count * (1 if span <= 256 else 2 if span <= 65536 else 4)
                      for (_, count), span in source_spans.items())
    maximum_corners = max((count for _, count in unique_ranges), default=0)
    corner_shift = max(0, maximum_corners - 1).bit_length()
    compatible = {tuple(map(int, batch[[0, 1, 2, 3, 4, 5, 11]])) for batch in batches}
    columns = ("material", "layout", "pipeline", "motion", "passes", "instances",
               "surface_kind")
    state_costs = [dict(zip(columns, key), **{**state, "batches": len(state["batches"])})
                   for key, state in states.items()]
    state_costs.sort(key=lambda row: (-row["main_selected_indices"], row["material"],
                                     row["layout"], row["pipeline"]))
    return {
        "generation": metadata["generation"],
        "scope": metadata["scope"],
        "batches": len(batches),
        "compatible_state_groups": len(compatible),
        "roots": len(roots),
        "logical_clusters": all_clusters,
        "unique_source_ranges": len(unique_ranges),
        "selected_clusters": selected_clusters,
        "selected_indices": int(selected.sum()),
        "state_costs": state_costs,
        "selected_cluster_vertex_references": selected_vertex_references,
        "unique_vertices_per_cluster_max": maximum,
        "unique_vertices_per_cluster_p50_p95_p99": (
            np.percentile(unique_counts, [50, 95, 99]).tolist() if len(unique_counts) else []
        ),
        "source_vertex_span_max": int(span_counts.max()) if len(span_counts) else 0,
        "source_vertex_span_p50_p95_p99": (
            np.percentile(span_counts, [50, 95, 99]).tolist() if len(span_counts) else []
        ),
        "cluster_base_and_delta_index_bytes": delta_bytes,
        "virtual_unique_vertex_local_bits": shift,
        "maximum_corners_per_cluster": maximum_corners,
        "virtual_corner_local_bits": corner_shift,
        "virtual_corner_cluster_slots_u32": 1 << (32 - corner_shift) if corner_shift <= 32 else 0,
        "source_index_bytes_unique_ranges": index_count * 4,
        "packed_local_index_bytes": index_count * local_bytes,
        "source_vertex_reference_bytes": reference_count * 4,
        "packed_reference_and_index_bytes": index_count * local_bytes + reference_count * 4,
        "local_index_element_bytes": local_bytes,
        "resident_source_buffer_prefix_bytes": source.nbytes,
        "note": "Costs exclude vertices, metadata and runtime compaction; ranges may overlap.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    print(json.dumps(measure(*load(args.directory)), indent=2))


if __name__ == "__main__":
    main()
