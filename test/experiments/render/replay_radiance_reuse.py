"""Check field reuse and invalidation in the native relighting consumer."""

import argparse
import hashlib
import json
from pathlib import Path

from replay_building_radiance import ROOT, compress, image
from replay_cached_radiance import run
from replay_source_view_misses import backup


def replay(args):
    if args.output.resolve().is_relative_to(ROOT):
        raise ValueError("put private outputs outside the checkout")
    args.output.mkdir(parents=True, exist_ok=False)
    request = json.loads((args.warm / "input.json").read_text())
    if Path(request["source_input"]).exists():
        raise ValueError("the warm input must not have source data")
    controls = request["queries"]
    a = {**controls[0], "dump_source_field": False, "sun_elevation_deg": 45}
    b = {**a, "sun_elevation_deg": 5}
    moved = {**controls[1], "dump_source_field": False, "sun_elevation_deg": 45}
    focal = {**moved, "focal_px": moved["focal_px"] * 1.5}
    snapshot = {**focal, "dump_source_field": True}
    changed_view = {**controls[2], "dump_source_field": False}
    cases = [
        ("a", a),
        ("a-repeat", a),
        ("b", b),
        ("b-repeat", b),
        ("a-again", a),
        ("moved", moved),
        ("moved-repeat", moved),
        ("focal", focal),
        ("focal-repeat", focal),
        ("snapshot", snapshot),
        ("snapshot-repeat", snapshot),
        ("changed-view", changed_view),
    ]
    request["queries"] = [{**query, "name": name} for name, query in cases]
    request["source_request"]["source_tests"] = 0
    backup(
        args.warm / "world-view-assets.sqlite", args.output / "world-view-assets.sqlite"
    )
    report = run(args.client.resolve(), args.output, request, True)
    expected = [
        False,
        True,
        False,
        True,
        False,
        False,
        True,
        False,
        True,
        False,
        True,
        False,
    ]
    if [case["field_reused"] for case in report["cases"]] != expected:
        raise ValueError("field reuse did not follow all changed producer inputs")
    if (report["field_renders"], report["field_reuses"], report["field_writes"]) != (
        7,
        5,
        0,
    ):
        raise ValueError("unexpected render/reuse/write counts")
    if (
        report["render_device_creations"] != 1
        or report["captures"]
        or "source_capture" in report
    ):
        raise ValueError("reuse cannot create a source owner or another render device")
    for case in report["cases"]:
        if case["field_reused"] and any(
            case[key] != 0
            for key in (
                "field_prepare_ms",
                "field_gpu_ms",
                "field_setup_gpu_ms",
                "field_load_ms",
                "source_readback_bytes",
            )
        ):
            raise ValueError(
                "a reused field repeated loading, rendering or source readback"
            )
    endings = (".linear", ".depth", ".rgba", ".ss16")
    pairs = [
        ("a", "a-repeat"),
        ("a", "a-again"),
        ("b", "b-repeat"),
        ("moved", "moved-repeat"),
        ("focal", "focal-repeat"),
        ("snapshot", "snapshot-repeat"),
    ]
    hashes = []
    for name, _ in cases:
        image(args.output / (name + "-radiance-world.linear"), 1280 * 720)
    for before, after in pairs:
        for ending in endings:
            first = (args.output / (before + "-radiance-world" + ending)).read_bytes()
            second = (args.output / (after + "-radiance-world" + ending)).read_bytes()
            if first != second:
                raise ValueError(f"field reuse changed {before}/{after}{ending}")
            hashes.append(
                {
                    "pair": [before, after],
                    "type": ending,
                    "sha256": hashlib.sha256(first).hexdigest(),
                }
            )
    if (args.output / "a-radiance-world.linear").read_bytes() == (
        args.output / "b-radiance-world.linear"
    ).read_bytes():
        raise ValueError("light invalidation produced no visible response")
    before = (args.output / "snapshot-source-field.f32").read_bytes()
    if before != (args.output / "snapshot-repeat-source-field.f32").read_bytes():
        raise ValueError("a retained diagnostic snapshot changed")
    receipt = {
        "field_renders": 7,
        "field_reuses": 5,
        "source_work": False,
        "pairs": hashes,
        "artifacts": compress(args.output),
        "scope": "One retained native field; no general World cache/LOD/motion budget admission.",
    }
    (args.output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(f"radiance reuse passed: {args.output / 'receipt.json'}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--warm", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    replay(parser.parse_args())
