"""Compare required road-profile events with redundant uniform-grid overlays."""

import argparse
import json
import math
import struct
import tempfile
import time
from pathlib import Path

import numpy as np

HEADER = struct.Struct("<QQddd")
MARKER = 0x53414D504C453031
MAX_STATIONS = 262144


def requirements(start, end, step, required):
    values = np.asarray(required, dtype=np.float64)
    if (
        not all(map(math.isfinite, (start, end, step)))
        or not end > start
        or not step > 0
        or len(values) > MAX_STATIONS
        or not np.isfinite(values).all()
        or np.any(values[1:] < values[:-1])
        or np.any(values < start)
        or np.any(values > end)
    ):
        raise ValueError("invalid interval or required stations")
    quotient = (end - start) / step
    if not math.isfinite(quotient) or quotient >= MAX_STATIONS:
        raise ValueError("sampling exceeds its input budget")
    return values


def original(start, end, step, required):
    required = requirements(start, end, step, required)
    whole = int((end - start) / step)
    uniform = start + np.arange(whole + 1, dtype=np.float64) * step
    if uniform[-1] != end:
        uniform = np.r_[uniform, end]
    result = np.unique(np.r_[uniform, required])
    if len(result) > MAX_STATIONS:
        raise ValueError("combined sampling exceeds its generation budget")
    return result


def fill_gaps(start, end, step, required):
    required = requirements(start, end, step, required)
    result = [start]
    for right in np.r_[required, end]:
        left = result[-1]
        if right == left:
            continue
        number = int((right - left) / step)
        for offset in range(1, number + 1):
            at = left + offset * step
            if at >= right:
                break
            if at <= result[-1]:
                raise ValueError("sampling cannot advance at this precision")
            result.append(at)
            if len(result) >= MAX_STATIONS:
                raise ValueError("sampling exceeds its generation budget")
        result.append(float(right))
        if len(result) > MAX_STATIONS:
            raise ValueError("sampling exceeds its generation budget")
    return np.asarray(result)


def captured(path):
    if path.stat().st_size > 128 << 20:
        raise ValueError("capture exceeds its byte budget")
    raw = path.read_bytes()
    at = 0
    while at < len(raw):
        if len(raw) - at < HEADER.size:
            raise ValueError("truncated capture header")
        marker, count, start, end, step = HEADER.unpack_from(raw, at)
        at += HEADER.size
        if marker != MARKER or count > MAX_STATIONS or count * 8 > len(raw) - at:
            raise ValueError("invalid capture record")
        values = np.frombuffer(raw, dtype="<f8", count=count, offset=at)
        at += count * 8
        yield start, end, step, values


def compare(records):
    count = old_total = new_total = worse = maximum = 0
    worst_gap_excess = 0.0
    examples = []
    for start, end, step, required in records:
        old = original(start, end, step, required)
        new = fill_gaps(start, end, step, required)
        if (
            new[0] != start
            or new[-1] != end
            or not np.isfinite(new).all()
            or np.any(new[1:] <= new[:-1])
            or not np.isin(required, new).all()
        ):
            raise ValueError("candidate loses an event or a valid interval")
        excess = float(np.max(np.diff(new)) - step)
        scale = max(abs(start), abs(end), abs(step), 1.0)
        if excess > 4 * math.ulp(scale):
            raise ValueError("candidate exceeds the maximum gap")
        worst_gap_excess = max(worst_gap_excess, excess)
        count += 1
        old_total += len(old)
        new_total += len(new)
        maximum = max(maximum, len(new))
        worse += len(new) > len(old)
        if len(examples) < 12 and len(old) > len(new) + 2:
            examples.append(
                {
                    "start": start,
                    "end": end,
                    "step": step,
                    "required": required.tolist(),
                }
            )
    if not count:
        raise ValueError("empty road capture")
    return {
        "scope": "Fixed actual station requests; no surface-error, image or frame admission",
        "records": count,
        "original_stations": old_total,
        "candidate_stations": new_total,
        "reduction": (old_total - new_total) / old_total,
        "larger_records": worse,
        "maximum_candidate_stations": maximum,
        "maximum_float_gap_excess_m": worst_gap_excess,
        "representative_requests": examples,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("write diagnostic results into the system temporary directory")
    before = time.perf_counter()
    result = compare(captured(args.input))
    result["elapsed_s"] = time.perf_counter() - before
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps({k: v for k, v in result.items() if k != "representative_requests"})
    )


if __name__ == "__main__":
    main()
