"""Validate native Vulkan work records; undefined stages remain unmeasured."""

import hashlib
from collections import defaultdict
import math

ABSENT = 2**32 - 1
COUNTERS = ("ia_vertices", "ia_primitives", "vertex_invocations", "fragment_invocations", "compute_invocations")


def boolean(value):
    if value not in ("true", "false"):
        raise ValueError("invalid availability flag")
    return value == "true"


def interval(begin, end, bits, period):
    if not 0 < bits <= 64 or not math.isfinite(period) or period <= 0:
        raise ValueError("invalid timestamp properties")
    return ((end - begin) & ((1 << bits) - 1)) * period


def sample(fields):
    if len(fields) != 23:
        raise ValueError("wrong sample field count")
    result = {
        "frame": int(fields[1]),
        "index": int(fields[2]),
        "kind": int(fields[3]),
        "parent": int(fields[4]),
        "calls": int(fields[5]),
        "label": fields[6],
        "missing_pipeline": boolean(fields[7]),
        "time_available": boolean(fields[8]),
        "statistics_available": boolean(fields[9]),
        "fragment_present": boolean(fields[10]),
        "begin": int(fields[11]),
        "end": int(fields[12]),
        "period": float(fields[13]),
        "bits": int(fields[14]),
        "workgroups": [int(x) for x in fields[20:23]],
    }
    result.update(zip(COUNTERS, map(int, fields[15:20]), strict=True))
    if result["kind"] not in (0, 1, 2) or any(result[x] < 0 for x in COUNTERS):
        raise ValueError("invalid sample kind or counter")
    result["elapsed_ns"] = interval(result["begin"], result["end"], result["bits"], result["period"]) if result["time_available"] else None
    undefined = set()
    if not result["statistics_available"] or not result["calls"]:
        undefined.update(COUNTERS)
    elif result["kind"] == 2:
        undefined.update(COUNTERS[:-1])
    else:
        undefined.add("compute_invocations")
        if not result["fragment_present"]:
            undefined.add("fragment_invocations")
    for name in undefined:
        result[name] = None
    return result


def parse(text):
    frames, samples, compute, costs = {}, [], [], []
    row = gpu = None
    for line in text.splitlines():
        fields = line.split("\t")
        tag = fields[0]
        if tag == "ROW":
            if row is not None:
                raise ValueError("duplicate ROW")
            row = fields
        elif tag == "GPUROW":
            if gpu is not None:
                raise ValueError("duplicate GPUROW")
            gpu = fields
        elif tag == "VK_WORK_ERROR":
            raise ValueError(line)
        elif tag == "VK_WORK_FRAME":
            serial = int(fields[1])
            if serial in frames:
                raise ValueError("duplicate frame")
            frames[serial] = {"serial": serial, "sample_count": int(fields[2]), "omitted": int(fields[3]), "submission_ns": float(fields[4]), "compute_count": int(fields[5]), "compute_truncated": boolean(fields[6])}
        elif tag == "VK_WORK_SAMPLE":
            samples.append(sample(fields))
        elif tag == "VK_WORK_COMPUTE":
            compute.append(fields)
        elif tag == "VK_WORK_COST":
            costs.append({"calls": int(fields[1]), "hook_ns": float(fields[2]), "cold_output_ns": float(fields[3]), "captures": int(fields[4]), "omitted_captures": int(fields[5]), "cancelled_captures": int(fields[6]), "bytes_per_trace": int(fields[7])})
    return frames, samples, compute, costs, row, gpu


def validate_samples(frames, samples, compute):
    indexed = {(x["frame"], x["index"]): x for x in samples}
    if len(indexed) != len(samples):
        raise ValueError("duplicate sample")
    by_frame = defaultdict(list)
    compute_by_frame = defaultdict(list)
    parents = {(x["frame"], x["parent"]) for x in samples if x["parent"] != ABSENT}
    for entry in samples:
        by_frame[entry["frame"]].append(entry)
    for entry in compute:
        serial = int(entry[1])
        if serial not in frames:
            raise ValueError("unknown compute submission")
        compute_by_frame[serial].append(entry)
    for frame in frames.values():
        actual = by_frame[frame["serial"]]
        if len(actual) != frame["sample_count"] or {x["index"] for x in actual} != set(range(frame["sample_count"])):
            raise ValueError("missing samples")
        if frame["omitted"] or frame["compute_truncated"]:
            raise ValueError("truncated frame")
        existing = compute_by_frame[frame["serial"]]
        if len(existing) != frame["compute_count"] or {int(x[2]) for x in existing} != set(range(frame["compute_count"])) or any(x[3] != "true" for x in existing):
            raise ValueError("missing compute timings")
        if sum(x["kind"] == 2 for x in actual) != frame["compute_count"]:
            raise ValueError("missing compute work")
    for entry in samples:
        if entry["frame"] not in frames or entry["missing_pipeline"] or not entry["time_available"]:
            raise ValueError("unavailable or unassigned sample")
        if entry["parent"] != ABSENT:
            parent = indexed.get((entry["frame"], entry["parent"]))
            if parent is None or parent["kind"] != 0 or entry["kind"] != 1:
                raise ValueError("invalid parent")
            if (entry["bits"], entry["period"]) != (parent["bits"], parent["period"]):
                raise ValueError("timestamp mismatch")
            if interval(parent["begin"], entry["begin"], entry["bits"], entry["period"]) + entry["elapsed_ns"] > parent["elapsed_ns"]:
                raise ValueError("child outside parent interval")
        has_children = (entry["frame"], entry["index"]) in parents
        if entry["calls"] and not entry["statistics_available"] and not (entry["kind"] == 0 and has_children):
            raise ValueError("missing statistics")


def analyze(path, expected_frames=60):
    frames, samples, compute, costs, row, gpu = parse(path.read_text())
    if not frames or not samples or not costs:
        raise ValueError("missing capture")
    validate_samples(frames, samples, compute)
    if sum(x["captures"] for x in costs) != len(frames):
        raise ValueError("capture count differs from reported frames")
    if any(x["omitted_captures"] for x in costs):
        raise ValueError("truncated captures")
    skyframes = {x["frame"] for x in samples if "sky.frag.spv" in x["label"]}
    if not row or not gpu:
        raise ValueError("missing frame summaries")
    expected = expected_frames
    if expected <= 0 or row[3] != "1" or int(row[7]) != expected or row[18] != "360" or list(map(int, gpu[2:4])) != [expected, expected] or len(skyframes) != expected:
        raise ValueError("incomplete measured window")
    submission_sum = sum(frames[k]["submission_ns"] for k in skyframes)
    if not math.isfinite(submission_sum) or abs(submission_sum - float(gpu[4]) * 1e6) > 1:
        raise ValueError("submission times differ from GPUROW")
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    return {
        "scope": "Instrumented diagnostic only. Undefined stage counters are null. Intervals overlap; do not add durations. Calls denote recorded DrawPackets, not expanded indirect draws. No frame gate.",
        "frames": list(frames.values()),
        "sample_count": len(samples),
        "sky_frame_count": len(skyframes),
        "sky_submission_sum_ns": submission_sum,
        "ROW": row,
        "GPUROW": gpu,
        "costs": costs,
        "samples": samples,
        "existing_compute_rows": compute,
        "log_sha256": digest,
    }
