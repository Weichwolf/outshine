#!/usr/bin/env python3
"""Check a complete route-camera TSV against explicit metre/radian budgets."""

import argparse
import csv
import json
import math
from dataclasses import dataclass


@dataclass(frozen=True)
class Limits:
    duration_s: float
    step_s: float
    position_step_m: float
    rotation_step_rad: float
    station_step_m: float
    closure_position_m: float = None
    closure_rotation_rad: float = None
    minimum_clearance_m: float = 0.0
    lap_length_m: float = None


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def rotation(a, b):
    relative = [[dot(x, y) for y in b] for x in a]
    trace = sum(relative[axis][axis] for axis in range(3))
    skew = (relative[2][1] - relative[1][2], relative[0][2] - relative[2][0],
            relative[1][0] - relative[0][1])
    return math.atan2(math.sqrt(dot(skew, skew)) / 2.0, (trace - 1.0) / 2.0)


def evaluate(path, limits):
    for value in (limits.duration_s, limits.step_s, limits.position_step_m,
                  limits.rotation_step_rad, limits.station_step_m):
        if not math.isfinite(value) or value <= 0:
            raise ValueError("duration, step and movement budgets must be finite and positive")
    closures = (limits.closure_position_m, limits.closure_rotation_rad)
    if (limits.lap_length_m is None) != (closures[0] is None):
        raise ValueError("closed laps require length and both pose closure budgets")
    if limits.lap_length_m is not None and (not math.isfinite(limits.lap_length_m) or limits.lap_length_m <= 0):
        raise ValueError("lap length must be finite and positive")
    if (closures[0] is None) != (closures[1] is None):
        raise ValueError("both closure budgets are required for a closed lap")
    for value in (*closures, limits.minimum_clearance_m):
        if value is not None and (not math.isfinite(value) or value < 0):
            raise ValueError("closure and clearance budgets must be finite and nonnegative")
    steps = limits.duration_s / limits.step_s
    if not math.isfinite(steps):
        raise ValueError("duration/step ratio is not representable")
    expected = round(steps)
    if expected < 2 or abs(expected * limits.step_s - limits.duration_s) > 1e-6:
        raise ValueError("duration must contain at least two whole simulation steps")
    first = previous = None
    count = 0
    maximum_position = maximum_rotation = maximum_station = 0.0
    with open(path, newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        if not reader.fieldnames or len(set(reader.fieldnames)) != len(reader.fieldnames):
            raise ValueError("missing or duplicate TSV columns")
        for count, row in enumerate(reader, 1):
            if count > expected:
                raise ValueError("trace exceeds declared duration")
            def number(name):
                try:
                    value = float(row[name])
                except (KeyError, TypeError, ValueError) as error:
                    raise ValueError(f"frame {count}: missing/invalid {name}") from error
                if not math.isfinite(value):
                    raise ValueError(f"frame {count}: nonfinite {name}")
                return value

            time = number("time_s")
            if abs(time - count * limits.step_s) > 1e-6:
                raise ValueError(f"frame {count}: missing, duplicate or mistimed tick")
            station = number("station_m")
            serial = number("camera_frame_serial")
            if station < 0 or serial < 1 or serial != math.floor(serial):
                raise ValueError(f"frame {count}: invalid station/frame serial")
            eye = tuple(number(f"camera_eye_{axis}_m") for axis in ("east", "up", "south"))
            forward = tuple(number(f"camera_forward_{axis}") for axis in ("east", "up", "south"))
            up = tuple(number(f"camera_up_{axis}") for axis in ("east", "up", "south"))
            if max(abs(dot(forward, forward) - 1), abs(dot(up, up) - 1),
                   abs(dot(forward, up))) > 1e-8:
                raise ValueError(f"frame {count}: camera basis is not orthonormal")
            forward = tuple(value / math.sqrt(dot(forward, forward)) for value in forward)
            projection = dot(forward, up)
            up = tuple(value - projection * axis for value, axis in zip(up, forward))
            up = tuple(value / math.sqrt(dot(up, up)) for value in up)
            basis = (cross(forward, up), up, forward)
            for track in ("left", "center", "right"):
                if number(f"{track}_contact") != 1:
                    raise ValueError(f"frame {count}: missing {track} road contact")
            if number("eye_clearance_m") < limits.minimum_clearance_m:
                raise ValueError(f"frame {count}: insufficient camera clearance")
            current = (station, serial, eye, basis)
            if previous:
                if station < previous[0] or serial <= previous[1]:
                    raise ValueError(f"frame {count}: station regressed or frame was reused")
                station_step = station - previous[0]
                maximum_station = max(maximum_station, station_step)
                if station_step > limits.station_step_m:
                    raise ValueError(f"frame {count}: station step exceeds budget")
                position_step = math.dist(eye, previous[2])
                rotation_step = rotation(basis, previous[3])
                maximum_position = max(maximum_position, position_step)
                maximum_rotation = max(maximum_rotation, rotation_step)
                if position_step > limits.position_step_m or rotation_step > limits.rotation_step_rad:
                    raise ValueError(f"frame {count}: position/rotation step exceeds budget")
            else:
                first = current
            previous = current
    if count != expected:
        raise ValueError(f"incomplete trace: {count} frames, expected {expected}")
    closure_position = math.dist(first[2], previous[2])
    closure_rotation = rotation(first[3], previous[3])
    if limits.lap_length_m is not None and abs(previous[0] - limits.lap_length_m) > closures[0]:
        raise ValueError("route station has not completed the declared lap")
    if closures[0] is not None and (closure_position > closures[0] or closure_rotation > closures[1]):
        raise ValueError("lap position/orientation does not close within budget")
    return {"frames": count, "maximum_position_step_m": maximum_position,
            "maximum_rotation_step_rad": maximum_rotation, "maximum_station_step_m": maximum_station,
            "closure_position_m": closure_position, "closure_rotation_rad": closure_rotation}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace")
    for name in ("duration-s", "step-s", "position-step-m", "rotation-step-rad", "station-step-m"):
        parser.add_argument("--" + name, type=float, required=True)
    for name in ("closure-position-m", "closure-rotation-rad", "lap-length-m"):
        parser.add_argument("--" + name, type=float)
    parser.add_argument("--minimum-clearance-m", type=float, default=0.0)
    args = vars(parser.parse_args())
    path = args.pop("trace")
    try:
        print(json.dumps(evaluate(path, Limits(**args)), sort_keys=True))
    except (OSError, ValueError) as error:
        parser.exit(1, f"motion trace: FAIL: {error}\n")


if __name__ == "__main__":
    main()
