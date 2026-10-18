"""Check actual long-phase sky frames against the independent world-field integral."""

import argparse
import json
from pathlib import Path

import numpy as np
from cloud_sky_reference import read
from cloud_view_reference import physical_air, transfer


def check_frame(metadata):
    case, images, rays, medium, weather = read(metadata)
    if not case["boundary_sky"] or case["weather_frame_bytes"] != 160:
        raise AssertionError(
            "the actual sky capture must consume112B plus unchanged48B lighting/eye"
        )
    if int(weather[1, 1]) != case["cloud_profile"]:
        raise AssertionError("the actual packed profile must match its requested input")
    active = np.linalg.norm(rays[:, 3:6], axis=1) > 0
    if not np.all(active) or np.any(images[2, :, 3] != 0):
        raise AssertionError(
            "the selected sky-only case must contain every requested sky ray"
        )
    queries = rays.copy()
    _, endpoints = physical_air(rays, medium)
    queries[:, 10] = endpoints * 0.001
    expected = transfer(
        queries, medium, weather, case["phase_s"], profile=case["cloud_profile"]
    )
    actual = np.c_[images[2, :, :3], images[3, :, :3]].astype(float)
    error = np.abs(actual - expected)
    composed = images[0, :, :3] * expected[:, 3:] + expected[:, :3]
    composition = float(np.max(abs(images[1, :, :3] - composed)))
    endpoint = float(np.max(abs(images[3, :, 3].astype(float) * 1000.0 - endpoints)))
    if (
        not np.all(np.isfinite(images))
        or np.max(error) > 1e-4
        or composition > 1e-4
        or endpoint > 1.0
    ):
        raise AssertionError(
            f"{case['name']}: transfer={error.max()}, composition={composition}, endpoint={endpoint}"
        )
    np.testing.assert_array_equal(
        images[1, :, 3].view("<u4"), images[0, :, 3].view("<u4")
    )
    return (
        case,
        images,
        rays,
        medium,
        weather,
        actual,
        expected,
        {
            "case": case["name"],
            "profile": [case["width"], case["height"]],
            "cloud_profile": case["cloud_profile"],
            "phase_s": case["phase_s"],
            "max_radiance_error": float(np.max(error[:, :3])),
            "max_transmittance_error": float(np.max(error[:, 3:])),
            "max_composition_error": composition,
            "max_endpoint_error_m": endpoint,
            "work_counters": images[4].astype(float).sum(axis=0).tolist(),
        },
    )


def cross_cases(frames):
    steps, anchors = [], []
    for profile in (0, 1):
        for sign in (-1, 1):
            prefix = f"wind-{profile}-{sign}"
            base = frames[prefix + "-0"]
            for suffix in ("30", "60", "anchor"):
                now = frames[prefix + "-" + suffix]
                for old, current in (
                    (base[2], now[2]),
                    (base[3], now[3]),
                    (base[4][7:], now[4][7:]),
                ):
                    np.testing.assert_array_equal(old.view("<u1"), current.view("<u1"))
                if suffix == "anchor":
                    error = float(np.max(abs(now[5] - base[5])))
                    if error > 1e-4:
                        raise AssertionError(
                            "a field anchor cannot move the physical sky or its density field"
                        )
                    anchors.append(
                        {
                            "case": prefix,
                            "max_transfer_change": error,
                            "physical_rays_byte_equal": True,
                        }
                    )
                    continue
                actual_step = now[5] - base[5]
                reference_step = now[6] - base[6]
                changed = int(
                    np.count_nonzero(
                        np.any(now[1][1, :, :3] != base[1][1, :, :3], axis=1)
                    )
                )
                norm = float(np.linalg.norm(reference_step))
                if not changed or norm == 0:
                    raise AssertionError(
                        "individual30/60Hz wind steps must advance the actual sky"
                    )
                steps.append(
                    {
                        "case": prefix,
                        "rate_hz": int(suffix),
                        "changed_rgb_pixels": changed,
                        "relative_transfer_step_l2_error": float(
                            np.linalg.norm(actual_step - reference_step) / norm
                        ),
                    }
                )
    return steps, anchors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    args = parser.parse_args()
    frames = {}
    for metadata in sorted(args.folder.glob("actual.wind-*.json")):
        frame = check_frame(metadata)
        frames[frame[0]["name"]] = frame
        print(json.dumps(frame[-1]), flush=True)
    if len(frames) != 16:
        raise AssertionError(
            "all16 requested profile/phase/rate/anchor cases must exist"
        )
    steps, anchors = cross_cases(frames)
    record = {
        "cases": [f[-1] for f in frames.values()],
        "steps": steps,
        "anchors": anchors,
        "limits": {
            "transfer_absolute": 1e-4,
            "composition_absolute": 1e-4,
            "sky_endpoint_m": 1.0,
        },
        "scope": "All captured pixels independently checked. Sky step relative L2 is measured, not substituted for the separate unchanged5% point-density gate; no world/history/device acceptance.",
    }
    (args.folder / "wind-reference.json").write_text(
        json.dumps(record, indent=2) + "\n"
    )


if __name__ == "__main__":
    main()
