"""Compare matched canopy packages using the existing Vulkan work analyzer."""

import argparse
import hashlib
import importlib.util
import json
import statistics
import tempfile
from collections import defaultdict
from pathlib import Path

MODES = ("legacy", "nine", "adaptive")
KINDS = ("plain", "probe")
COUNTERS = (
    "ia_vertices",
    "ia_primitives",
    "vertex_invocations",
    "fragment_invocations",
    "compute_invocations",
)


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def measured_work(data):
    frames = sorted(
        {r["frame"] for r in data["samples"] if "sky.frag.spv" in r["label"]}
    )
    order = {serial: index for index, serial in enumerate(frames)}
    meta = {r["serial"]: r for r in data["frames"]}
    times = [meta[serial]["submission_ns"] / 1e6 for serial in frames]
    groups, indexed = defaultdict(list), {}
    for row in data["samples"]:
        if row["frame"] in order:
            indexed[(order[row["frame"]], row["index"])] = row
            groups[(row["kind"], row["label"])].append(row)
    summaries = []
    for (kind, label), rows in groups.items():
        elapsed = [r["elapsed_ns"] / 1e6 for r in rows]
        item = {
            "kind": kind,
            "label": label,
            "count": len(rows),
            "median_ms": statistics.median(elapsed),
            "max_ms": max(elapsed),
        }
        for counter in COUNTERS:
            values = [r[counter] for r in rows if r[counter] is not None]
            item[counter + "_sum"] = sum(values) if values else None
        summaries.append(item)
    series = {}
    for name, kind, token in (
        ("ground", 1, "groundPatchLattice"),
        ("objects", 0, "fsLit"),
    ):
        series[name] = [
            max(
                (
                    r["elapsed_ns"] / 1e6
                    for (at, _), r in indexed.items()
                    if at == index and r["kind"] == kind and token in r["label"]
                ),
                default=None,
            )
            for index in range(len(times))
        ]
    worst = max(range(len(times)), key=lambda index: times[index])
    largest = sorted(
        (r for (index, _), r in indexed.items() if index == worst),
        key=lambda row: row["elapsed_ns"],
        reverse=True,
    )[:6]
    return {
        "frame_ms": times,
        "interval_series": series,
        "worst_frame_index": worst,
        "groups": summaries,
        "worst_frame_samples": largest,
        "costs": data["costs"],
    }, indexed


def case(source, place, mode, kind, analyzer, fps):
    stem = f"{place}-{mode}-{kind}"
    log, image = source / (stem + ".log"), source / (stem + ".png")
    text = log.read_text()
    _, _, _, _, row, gpu = analyzer.parse(text)
    profile = next(
        line.split("\t") for line in text.splitlines() if line.startswith("PROFILE\t")
    )
    assert profile[1] == row[1] == gpu[1] == place
    assert profile[2:5] == ["1920", "1080", str(fps)]
    assert row[3] == row[13] == "1" and int(row[7]) == fps and row[18] == "360"
    assert (
        int(row[11]) == 0
        and list(map(int, gpu[2:4])) == [fps, fps]
        and float(gpu[6]) == 0
    )
    record = {
        "host_p99_ms": float(row[6]),
        "preload_ms": float(row[17]),
        "turn_ms": float(row[16]),
        "gpu_max_ms": float(gpu[5]),
        "gpu_sum_ms": float(gpu[4]),
        "reported_triangles": int(row[10]),
        "bare_tiles": int(row[11]),
        "png_sha256": sha(image),
        "log_sha256": sha(log),
    }
    assert 0 < record["gpu_max_ms"] <= record["gpu_sum_ms"]
    assert all(record[key] >= 0 for key in ("host_p99_ms", "preload_ms", "turn_ms"))
    assert record["png_sha256"].startswith(row[2])
    if kind == "probe":
        saved = json.loads((source / (stem + ".work.json")).read_text())
        assert saved["log_sha256"] == record["log_sha256"]
        assert saved["image_sha256"] == record["png_sha256"]
    record["frame_gate"] = (
        None
        if kind == "probe"
        else (
            record["host_p99_ms"] <= 1000 / fps and record["gpu_max_ms"] <= 1000 / fps
        )
    )
    record["load_gate"] = None if kind == "probe" else record["preload_ms"] < 10000
    record["turn_gate"] = None if kind == "probe" else record["turn_ms"] <= 1000
    indexed = None
    if kind == "probe":
        data = analyzer.analyze(log, expected_frames=fps)
        record["diagnostic"], indexed = measured_work(data)
    return record, indexed


def compare_work(reference, candidate):
    differences = []
    for key in sorted(reference.keys() & candidate.keys()):
        for name in ("kind", "label", "calls", "workgroups", *COUNTERS):
            first, last = reference[key][name], candidate[key][name]
            if first != last:
                differences.append(
                    {
                        "frame_index": key[0],
                        "sample_index": key[1],
                        "counter": name,
                        "legacy": first,
                        "candidate": last,
                    }
                )
    return {
        "same_frame_sample_keys": reference.keys() == candidate.keys(),
        "different_fields": len(differences),
        "differences": differences,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--analyzer", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--places", nargs="+", default=["Koerbersee", "CentralPark", "Tokyo"]
    )
    parser.add_argument("--fps", type=int, choices=(30, 60), default=60)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(
        Path(__file__).resolve().parents[3]
    ) or not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("comparison output must be in system temp")
    spec = importlib.util.spec_from_file_location(
        "shared_vulkan_work_analysis", args.analyzer
    )
    analyzer = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(analyzer)
    report = {
        "scope": "Matched ordinary/diagnostic shader-package window; intervals overlap and "
        "must not be added. Only ordinary runs classify frame/load/turn gates. "
        "No isolated GPU or Metal/A18 acceptance; earlier red controls remain red.",
        "profile": {"width_px": 1920, "height_px": 1080, "fps": args.fps},
        "source": str(args.source.resolve()),
        "analyzer_sha256": sha(args.analyzer),
        "places": {},
    }
    for place in args.places:
        cases, work = {}, {}
        for mode in MODES:
            cases[mode] = {}
            for kind in KINDS:
                entry, indexed = case(
                    args.source, place, mode, kind, analyzer, args.fps
                )
                cases[mode][kind] = entry
                if indexed is not None:
                    work[mode] = indexed
            assert (
                cases[mode]["plain"]["png_sha256"] == cases[mode]["probe"]["png_sha256"]
            )
        report["places"][place] = {
            "cases": cases,
            "work_comparability": {
                mode: compare_work(work["legacy"], work[mode]) for mode in MODES[1:]
            },
        }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    print(f"{len(args.places) * 6} complete captures; ordinary/probe images exact")


if __name__ == "__main__":
    main()
