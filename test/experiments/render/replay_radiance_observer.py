"""Check native observer-dependent relighting without recapturing stored samples."""

import argparse
import copy
import json
import math
from pathlib import Path

from replay_building_radiance import ROOT, compress, image
from replay_cached_radiance import run
from replay_source_view_misses import backup


def turned(vector, angle):
    x, y, z = vector
    c, s = math.cos(angle), math.sin(angle)
    return [c * x + s * z, y, c * z - s * x]


def controls(request, capture):
    base = {
        **request["queries"][0],
        "filter_axis_samples": 1,
        "dump_source_field": True,
        "support_plane": "fitted",
        "field_lighting": "capture",
    }
    viewer = {**base, "field_lighting": "viewer"}
    moved = {**viewer, "eye": [viewer["eye"][0] + 4000, *viewer["eye"][1:]]}
    light = {**moved, "sun_elevation_deg": moved["sun_elevation_deg"] + 5}
    ortho = {**viewer, "capture": copy.deepcopy(capture)}
    shifted = copy.deepcopy(ortho)
    shifted["capture"]["eye_m"][0] += 100
    rotated = copy.deepcopy(ortho)
    for axis in ("right", "up", "forward"):
        rotated["capture"][axis] = turned(capture[axis], 0.05)
    default = {k: v for k, v in base.items() if k != "field_lighting"}
    cases = [
        ("capture", base, False),
        ("viewer", viewer, False),
        ("viewer-repeat", viewer, True),
        ("focal", {**viewer, "focal_px": viewer["focal_px"] * 1.2}, True),
        ("turn", {**viewer, "basis": [turned(v, 0.01) for v in viewer["basis"]]}, True),
        ("moved", moved, False),
        ("moved-repeat", moved, True),
        ("light", light, False),
        ("light-repeat", light, True),
        ("capture-again", base, False),
        ("capture-default", default, True),
        ("ortho", ortho, False),
        ("ortho-repeat", ortho, True),
        ("ortho-shift", shifted, True),
        ("ortho-turn", rotated, False),
        ("ortho-turn-repeat", rotated, True),
    ]
    return cases


def verify(folder, cases, report):
    expected = [reuse for _, _, reuse in cases]
    if report["scene_creations"] != 1 or report["rejected_handoffs"] != 1:
        raise ValueError(
            "field handoffs replaced the scene or missed the rollback control"
        )
    if any(
        c["scene_creations"] != 1 or c["piece_sources"] != 1 or c["piece_slots"] > 2
        for c in report["cases"]
    ):
        raise ValueError(
            "field publication grew live pieces or failed bounded slot reuse"
        )
    if [c["field_reused"] for c in report["cases"]] != expected:
        raise ValueError("relighting did not follow actual observer/light inputs")
    if (
        report["field_renders"],
        report["field_reuses"],
        report["field_source_uploads"],
        report["field_writes"],
    ) != (7, 9, 1, 0):
        raise ValueError("observer changes repeated source work or missed relighting")
    if (
        report["atlas_read_bytes"]
        or report["atlas_decodes"]
        or report["captures"]
        or report["render_device_creations"] != 1
    ):
        raise ValueError("fresh offline observer replay did source work")
    depth = (folder / "capture-source-depth.f32").read_bytes()
    camera = report["cases"][0]["source_depth"]
    for i, (name, _, _) in enumerate(cases):
        case = report["cases"][i]
        image(folder / (name + "-radiance-world.linear"), 1280 * 720)
        if (folder / (name + "-source-depth.f32")).read_bytes() != depth:
            raise ValueError("observer changed the fixed source depths")
        if case["source_depth"] != camera or case["field_source_reused"] != (i > 0):
            raise ValueError("observer changed the source rays or GPU source ownership")
        if i and case["support_fit_ms"] != 0:
            raise ValueError("observer repeated the source-only support fit")
    pairs = [("viewer", n) for n in ("viewer-repeat", "focal", "turn")]
    pairs += [
        ("moved", "moved-repeat"),
        ("light", "light-repeat"),
        ("capture", "capture-again"),
        ("capture", "capture-default"),
        ("ortho", "ortho-repeat"),
        ("ortho", "ortho-shift"),
        ("ortho-turn", "ortho-turn-repeat"),
    ]
    for a, b in pairs:
        if (folder / (a + "-source-field.f32")).read_bytes() != (
            folder / (b + "-source-field.f32")
        ).read_bytes():
            raise ValueError(f"identical lighting changed source radiance: {a}/{b}")
    for a, b in [
        ("capture", "viewer"),
        ("viewer", "moved"),
        ("moved", "light"),
        ("ortho", "ortho-turn"),
    ]:
        if (folder / (a + "-source-field.f32")).read_bytes() == (
            folder / (b + "-source-field.f32")
        ).read_bytes():
            raise ValueError(f"changed lighting produced no radiance response: {a}/{b}")
    return {
        "field_renders": 7,
        "field_reuses": 9,
        "field_source_uploads": 1,
        "scene_creations": 1,
        "rejected_handoffs": 1,
        "unchanged_depth_cases": len(cases),
        "equal_source_radiance_pairs": pairs,
    }


def reject_invalid_bases(args, request):
    viewer = next(q for q in request["queries"] if q["name"] == "viewer")
    rejected = []
    for mode in ("viewer", "capture"):
        good = {**viewer, "name": "valid", "field_lighting": mode}
        right, up = good["basis"][:2]
        variants = {
            "scaled": [2 * x for x in right],
            "mirrored": [-x for x in right],
            "sheared-unit": [
                (r + 0.5 * u) / math.sqrt(1.25) for r, u in zip(right, up)
            ],
        }
        for kind, axis in variants.items():
            bad = {**good, "name": "invalid-basis", "basis": [axis, *good["basis"][1:]]}
            for warm in (False, True):
                folder = (
                    args.output / f"basis-{mode}-{kind}-{'warm' if warm else 'cold'}"
                )
                folder.mkdir()
                backup(
                    args.output / "world-view-assets.sqlite",
                    folder / "world-view-assets.sqlite",
                )
                queries = [good, bad] if warm else [bad]
                run(
                    args.client.resolve(),
                    folder,
                    {**request, "queries": queries},
                    False,
                )
                if (
                    (folder / "terminal.exit").read_text().strip() != "1"
                    or "material field query camera has an invalid basis"
                    not in (folder / "terminal.log").read_text()
                    or (folder / "invalid-basis-radiance-world.linear").exists()
                ):
                    raise ValueError(
                        "invalid basis did not refuse consistently before publication"
                    )
                if warm:
                    image(folder / "valid-radiance-world.linear", 1280 * 720)
                rejected.append(
                    {
                        "mode": mode,
                        "kind": kind,
                        "warm": warm,
                        "artifacts": compress(folder),
                    }
                )
    return rejected


def replay(args):
    if args.output.resolve().is_relative_to(ROOT):
        raise ValueError("put private outputs outside the checkout")
    args.output.mkdir(parents=True, exist_ok=False)
    request = json.loads((args.warm / "input.json").read_text())
    warm = json.loads((args.warm / "radiance-result.json").read_text())
    cases = controls(request, warm["cases"][0]["source_depth"])
    request["queries"] = [{**q, "name": name} for name, q, _ in cases]
    request["source_input"] = str(args.output / "absent-source/input.json")
    backup(
        args.warm / "world-view-assets.sqlite", args.output / "world-view-assets.sqlite"
    )
    report = run(args.client.resolve(), args.output, request, True)
    receipt = verify(args.output, cases, report)
    invalid = args.output / "invalid-observer"
    invalid.mkdir()
    backup(
        args.output / "world-view-assets.sqlite", invalid / "world-view-assets.sqlite"
    )
    bad = {
        **request,
        "queries": [{**request["queries"][0], "field_lighting": "invalid"}],
    }
    run(args.client.resolve(), invalid, bad, False)
    if (
        "unknown material field lighting observer"
        not in (invalid / "terminal.log").read_text()
    ):
        raise ValueError(
            "invalid observer did not report its actual configuration error"
        )
    receipt["invalid_observer_rejected"] = True
    receipt["invalid_basis_cases"] = reject_invalid_bases(args, request)
    receipt["scope"] = "Native observer binding/reuse, no World/pose/budget admission"
    receipt["artifacts"] = compress(args.output)
    (args.output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(f"native observer replay passed: {args.output / 'receipt.json'}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--warm", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    replay(parser.parse_args())
