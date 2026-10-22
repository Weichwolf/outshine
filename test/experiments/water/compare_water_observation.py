"""Bind the later source observation to exact geometry, retaining every ambiguous match."""

import argparse
import json
import resource
import time
import tracemalloc
from collections import Counter, defaultdict
from pathlib import Path

import numpy as np
from native_river_inputs import read_v2, surface_rings
from native_water_observation import NO_INPUT, read_v3


def keys(data, kind):
    """Exact ordered coordinate/level bytes, not geometry tolerance or provider-ID guessing."""
    if kind == "surfaces":
        tiles = defaultdict(list)
        for tile in data["tiles"]:
            address = tuple(int(tile[n]) for n in ("zoom", "x", "y"))
            for index in range(
                int(tile["first_surface"]),
                int(tile["first_surface"] + tile["surface_count"]),
            ):
                tiles[index].append(address)
        return [
            (
                tuple(sorted(tiles[index])),
                row["level_m"].tobytes(),
                tuple(r.tobytes() for r in surface_rings(data, index)),
            )
            for index, row in enumerate(data[kind])
        ]
    return [
        (
            row["half_width_m"].tobytes(),
            data["points_lat_lon"][
                int(row["first_point"]) : int(row["first_point"] + row["point_count"])
            ].tobytes(),
            data["levels"][
                int(row["first_level"]) : int(row["first_level"] + row["point_count"])
            ].tobytes(),
        )
        for row in data[kind]
    ]


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def compare(original, observed, manifest, legacy):
    declared = legacy["source_semantics_observation"]
    if declared["water_sha256"] != manifest["observed_v3"]["sha256"]:
        raise ValueError(
            "recorded metadata observation belongs to different WATER3 bytes"
        )
    if (
        original["processed_tiles"] != observed["processed_tiles"]
        or original["counters"] != observed["counters"]
        or Counter(
            tuple(int(r[n]) for n in ("zoom", "x", "y", "surface_count"))
            for r in original["tiles"]
        )
        != Counter(
            tuple(int(r[n]) for n in ("zoom", "x", "y", "surface_count"))
            for r in observed["tiles"]
        )
    ):
        raise ValueError("original and observed coverage/counters differ")
    for index, source in declared["sources"].items():
        if canonical(source) != canonical(observed["sources"][int(index)]):
            raise ValueError("recorded source identity differs from native metadata")
    for pool in ("points_lat_lon", "levels"):
        if Counter(row.tobytes() for row in original[pool]) != Counter(
            row.tobytes() for row in observed[pool]
        ):
            raise ValueError(
                "original and observed scalar pools differ under exact permutation"
            )
    rows, metadata, work = {}, {}, {"record_keys": 0, "key_payload_bytes": 0}
    work["scalar_pool_records"] = sum(
        len(data[key])
        for data in (original, observed)
        for key in ("points_lat_lon", "levels")
    )
    work["scalar_pool_key_payload_bytes"] = sum(
        data[key].nbytes
        for data in (original, observed)
        for key in ("points_lat_lon", "levels")
    )
    for kind in ("surfaces", "courses"):
        old, new = keys(original, kind), keys(observed, kind)
        work["record_keys"] += len(old) + len(new)
        if kind == "surfaces":
            work["key_payload_bytes"] += sum(
                len(key[1]) + sum(map(len, key[2])) for key in old + new
            )
        else:
            work["key_payload_bytes"] += sum(sum(map(len, key)) for key in old + new)
        if Counter(old) != Counter(new):
            raise ValueError(
                f"original and observed {kind} differ under exact permutation"
            )
        lookup = defaultdict(list)
        for index, key in enumerate(new):
            lookup[key].append(index)
        rows[kind] = [
            {"original_index": index, "observed_indices": lookup[key]}
            for index, key in enumerate(old)
        ]
        selected = manifest["original_selection"][
            f"selected_{'surface' if kind == 'surfaces' else 'course'}_indices"
        ]
        if len(declared[kind]) != len(selected) or sorted(
            r["fixture_index"] for r in declared[kind]
        ) != sorted(selected):
            raise ValueError("recorded metadata selection is incomplete or duplicated")
        metadata[kind] = []
        for recorded in declared[kind]:
            index = int(recorded["fixture_index"])
            candidates = []
            for at in rows[kind][index]["observed_indices"]:
                binding = int(observed[kind][at]["input"])
                candidates.append(
                    None if binding == NO_INPUT else observed["inputs"][binding]
                )
            expected = {k: v for k, v in recorded.items() if k != "fixture_index"}
            if canonical(expected) not in {canonical(row) for row in candidates}:
                raise ValueError(
                    "recorded metadata has no exact native geometry/input binding"
                )
            metadata[kind].append(
                {
                    "original_index": index,
                    "observed_indices": rows[kind][index]["observed_indices"],
                    "candidate_inputs": candidates,
                    "unique_metadata": len({canonical(row) for row in candidates}) == 1,
                }
            )
    census = {
        kind: dict(
            Counter(row["kind"] for row in observed["inputs"] if row["layer"] == kind)
        )
        for kind in sorted({r["layer"] for r in observed["inputs"]})
    }
    return {
        "geometry_mapping": rows,
        "selected_observation_bindings": metadata,
        "source_census": census,
        "input_records": len(observed["inputs"]),
        "sources": observed["sources"],
        "work": work,
        "bridge_inputs": [i for i, r in enumerate(observed["inputs"]) if r["bridge"]],
        "tunnel_inputs": [i for i, r in enumerate(observed["inputs"]) if r["tunnel"]],
        "intermittent_unknown_inputs": [
            i for i, r in enumerate(observed["inputs"]) if r["intermittent"] is None
        ],
        "scope": "later observed metadata only; exact geometry equality grants no historical epoch or hydraulic connection",
    }


def diagnostic(observed, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import PathPatch
    from matplotlib.path import Path as PlotPath

    fig, ax = plt.subplots(figsize=(12.8, 7.2), dpi=100)
    colours = {"river": "#317ba7", "lake": "#46a49c", "pond": "#a39342"}
    for index, row in enumerate(observed["surfaces"]):
        binding = int(row["input"])
        kind = "unknown" if binding == NO_INPUT else observed["inputs"][binding]["kind"]
        vertices, codes = [], []
        for ring in surface_rings(observed, index):
            vertices.extend(ring[:, ::-1])
            codes.extend([PlotPath.MOVETO] + [PlotPath.LINETO] * (len(ring) - 1))
            vertices.append(ring[0, ::-1])
            codes.append(PlotPath.CLOSEPOLY)
        ax.add_patch(
            PathPatch(
                PlotPath(vertices, codes),
                facecolor=colours.get(kind, "#b169a8"),
                edgecolor="none",
            )
        )
    for row in observed["courses"]:
        binding = int(row["input"])
        elevated = binding != NO_INPUT and (
            observed["inputs"][binding]["bridge"]
            or observed["inputs"][binding]["tunnel"]
        )
        points = observed["points_lat_lon"][
            int(row["first_point"]) : int(row["first_point"] + row["point_count"])
        ]
        ax.plot(
            points[:, 1],
            points[:, 0],
            color="#b94030" if elevated else "#24668b",
            linewidth=1.0 if elevated else 0.3,
        )
    ax.autoscale_view()
    ax.set_aspect(1 / np.cos(np.deg2rad(observed["points_lat_lon"][:, 0].mean())))
    ax.set(
        xlabel="Longitude (degrees)",
        ylabel="Latitude (degrees)",
        title="Observed WATER3 classes and crossing flags; no inferred hydraulic joins",
    )
    fig.tight_layout()
    fig.savefig(output / "source-metadata.png")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        type=Path,
        default=Path(__file__).parent / "fixtures/feldkirch-ill-native-inputs.json",
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = json.loads(args.input.read_text())
    root = args.input.parent
    legacy = json.loads((root / manifest["fixture"]).read_text())
    start, cpu = time.perf_counter(), time.process_time()
    tracemalloc.start()
    decoded = [
        reader(root / manifest[k]["file"], manifest[k]["sha256"])
        for reader, k in ((read_v2, "historical_v2"), (read_v3, "observed_v3"))
    ]
    report = compare(*decoded, manifest, legacy)
    report["diagnostic_process"] = {
        "wall_s_excluding_plot": time.perf_counter() - start,
        "cpu_s_excluding_plot": time.process_time() - cpu,
        "tracemalloc_peak_bytes_excluding_plot": tracemalloc.get_traced_memory()[1],
        "source_bytes": sum(d["source_bytes"] for d in decoded),
        "allocation_count": None,
    }
    tracemalloc.stop()
    diagnostic(decoded[1], args.output)
    report["diagnostic_process"]["process_peak_rss_bytes_linux_including_plot"] = (
        resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "input_records": report["input_records"],
                "census": report["source_census"],
                "diagnostic_process": report["diagnostic_process"],
            }
        )
    )


if __name__ == "__main__":
    main()
