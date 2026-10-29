"""Replay a caller-provided timed eye path and export exact handoff opponents."""

import argparse
import hashlib
import json
from dataclasses import replace
from pathlib import Path

import numpy as np
from field_view_admission import ViewDomain, select_field
from field_view_inputs import read_view
from field_view_integrals import summed_field
from field_view_sampling import ImageView, reconstruct


def aligned(previous, current):
    low = np.minimum(previous["origin"], current["origin"])
    high = np.maximum(
        np.array(previous["origin"]) + previous["rgba"].shape[1::-1],
        np.array(current["origin"]) + current["rgba"].shape[1::-1],
    )
    arrays = []
    for sample in (previous, current):
        out = np.zeros((*(high - low)[::-1], 4))
        x, y = np.array(sample["origin"]) - low
        height, width = sample["rgba"].shape[:2]
        out[y : y + height, x : x + width] = sample["rgba"]
        arrays.append(out)
    return arrays


def replay(atlas, source, declaration):
    knots = declaration["knots"]
    times = np.array([k["time_s"] for k in knots], float)
    eyes = np.array([k["eye"] for k in knots], float)
    hz = declaration["samples_per_second"]
    if (
        len(knots) < 2
        or not np.isfinite(times).all()
        or not np.isfinite(eyes).all()
        or np.any(np.diff(times) <= 0)
        or not isinstance(hz, int)
        or not 1 <= hz <= 240
    ):
        raise ValueError("A bounded increasing timed eye path is required")
    count = int(np.ceil((times[-1] - times[0]) * hz)) + 1
    if count > 100000:
        raise ValueError("Motion-sample capacity exceeded before replay")
    first = declaration["camera"]
    reference = ImageView(
        tuple(eyes[0]), tuple(map(tuple, first["basis"])), first["focal_px"]
    )
    domain = ViewDomain(
        atlas["grid"],
        reference,
        declaration["source_binding"],
        declaration["radiance_binding"],
        **declaration["domain"],
    )
    field = summed_field(source)
    previous, was_field, transitions, field_frames = None, False, [], 0
    maximum_delta, maximum_relative_delta, maximum_pixels = 0.0, 0.0, 0
    for index in range(count):
        time_s = min(times[0] + index / hz, times[-1])
        eye = tuple(np.interp(time_s, times, eyes[:, axis]) for axis in range(3))
        view = replace(reference, eye=eye)
        mode, reason = select_field(
            domain,
            view,
            was_field=was_field,
            source_binding=domain.source_binding,
            radiance_binding=domain.radiance_binding,
            complete=declaration["complete"],
            depth_separated=declaration["depth_separated"],
            composition_qualified=declaration["composition_qualified"],
        )
        sample = reconstruct(field, atlas["grid"], view)
        maximum_pixels = max(maximum_pixels, sample["output_pixels"])
        field_frames += mode == "field"
        frame = {
            "index": index,
            "time_s": time_s,
            "eye": eye,
            "mode": mode,
            "reason": reason,
        }
        if previous is not None:
            a, b = aligned(previous["sample"], sample)
            maximum_delta = max(maximum_delta, float(abs(a - b).max()))
            mass = (a[:, :, :3].sum() + b[:, :, :3].sum()) / 2
            maximum_relative_delta = max(
                maximum_relative_delta,
                float(abs(a[:, :, :3] - b[:, :, :3]).sum() / mass) if mass else 0.0,
            )
            if was_field != (mode == "field"):
                transitions.append({"before": previous["frame"], "after": frame})
        previous, was_field = {"sample": sample, "frame": frame}, mode == "field"
    return {
        "samples": count,
        "field_frames": field_frames,
        "parent_frames": count - field_frames,
        "samples_per_second": hz,
        "duration_s": float(times[-1] - times[0]),
        "maximum_eye_speed_m_s": float(
            np.max(np.linalg.norm(np.diff(eyes, axis=0), axis=1) / np.diff(times))
        ),
        "prefix_builds": 1,
        "pose_prefix_rebuilds": 0,
        "maximum_output_pixels": maximum_pixels,
        "maximum_held_field_channel_step": maximum_delta,
        "maximum_held_field_rgb_relative_l1_step": maximum_relative_delta,
        "transitions": transitions,
        "scope": "Dense Python sampling and exclusive single-owner selection, not native frame performance. No original image oracle between native control poses. Exact before/after handoffs exported for separate native original comparison; parent must already be ready at handoff. No depth/shadow/world certificate.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("field", "trajectory", "output"):
        parser.add_argument("--" + key, type=Path, required=True)
    parser.add_argument("--held-query", required=True)
    args = parser.parse_args()
    declaration = json.loads(args.trajectory.read_text())
    states = json.loads((args.field / "radiance-result.json").read_text())["cases"]
    state = next(s for s in states if s["query"] == args.held_query)
    atlas = read_view(args.field / "world-view-assets.sqlite", state["field_asset_key"])
    raw = (args.field / (args.held_query + "-source-field.f32")).read_bytes()
    if (
        atlas["key"] != declaration["field_key"]
        or hashlib.sha256(raw).hexdigest() != declaration["radiance_binding"]
    ):
        raise ValueError("The timed path belongs to another held field")
    source = np.frombuffer(raw, dtype="<f4").reshape(
        atlas["pixels"], atlas["pixels"], 4
    )
    result = replay(atlas, source, declaration)
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    queries = []
    for i, transition in enumerate(result["transitions"]):
        for label in ("before", "after"):
            frame = transition[label]
            queries.append(
                {
                    "name": f"handoff-{i}-{label}",
                    "eye": frame["eye"],
                    **declaration["camera"],
                    "filter_axis_samples": 32,
                }
            )
    (args.output / "handoff-queries.json").write_text(
        json.dumps(queries, indent=2) + "\n"
    )
    print(json.dumps({k: v for k, v in result.items() if k != "transitions"}, indent=2))


if __name__ == "__main__":
    main()
