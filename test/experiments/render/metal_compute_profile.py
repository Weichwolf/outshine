"""Bounded Metal encoder intervals and declared dispatch work, not actual invocations."""

import re
from collections import defaultdict
from statistics import mean


def parse_compute_records(text):
    intervals, work = {}, []
    for line in text.splitlines():
        if not line.startswith(("METAL_COMPUTE ", "METAL_COMPUTE_WORK ")):
            continue
        fields, separator, pipeline = line.partition(" pipeline=")
        if not separator:
            raise ValueError("compute record has no pipeline identity")
        record = {
            key: int(value) for key, value in re.findall(r"(\w+)=(-?\d+)", fields)
        }
        record["pipeline"] = pipeline
        if line.startswith("METAL_COMPUTE_WORK "):
            if not {"serial", "pass", "declared_threads"} <= record.keys():
                raise ValueError("incomplete compute work record")
            work.append(record)
            continue
        required = {
            "serial",
            "pass",
            "valid",
            "duration_ns",
            "start_ns",
            "end_ns",
            "dispatches",
            "groups",
            "declared_threads",
            "unavailable_dispatches",
        }
        if (
            not required <= record.keys()
            or any(record[key] < 0 for key in required)
            or record["valid"] not in (0, 1)
        ):
            raise ValueError("incomplete or negative compute interval")
        key = record["serial"], record["pass"]
        if key in intervals:
            raise ValueError("duplicate compute interval")
        if record["valid"] and (
            record["start_ns"] <= 0
            or record["end_ns"] - record["start_ns"] != record["duration_ns"]
        ):
            raise ValueError("inconsistent compute timestamps")
        intervals[key] = record
    return intervals, work


def summarize_compute(intervals, work, frames):
    groups = defaultdict(list)
    for record in intervals.values():
        if record["serial"] in frames:
            groups[record["pipeline"] or "empty"].append(record)
    result = {}
    for name, rows in groups.items():
        valid = [r for r in rows if r["valid"]]
        missing = sum(r["unavailable_dispatches"] for r in rows)
        result[name] = {
            "samples": len(rows),
            "valid_timing_samples": len(valid),
            "duration_ms_mean": mean(r["duration_ns"] for r in valid) / 1e6
            if valid
            else None,
            "dispatches_mean": mean(r["dispatches"] for r in rows),
            "groups_mean": mean(r["groups"] for r in rows) if not missing else None,
            "declared_threads_mean": mean(r["declared_threads"] for r in rows)
            if not missing
            else None,
            "unavailable_dispatches": missing,
        }
        keys = {(r["serial"], r["pass"]) for r in rows}
        shaders = defaultdict(int)
        for record in work:
            if (record["serial"], record["pass"]) in keys:
                shaders[record["pipeline"]] += record["declared_threads"]
        result[name]["shader_declared_threads_mean"] = (
            {shader: count / len(rows) for shader, count in shaders.items()}
            if not missing
            else None
        )
    return result
