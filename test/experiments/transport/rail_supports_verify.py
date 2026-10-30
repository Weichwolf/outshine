"""Read saved whole rail supports against original surfaces, without constructing."""

import argparse
import json
from pathlib import Path

from camera_height_inputs import digest
from outshine_ref.transport.supports import SupportVolume
from rail_supports_context import DEFAULT_FIXTURE, load
from rail_supports_plan import VARIANTS, admit_station, footprint, spacing
from rail_supports_study import choose
from stair_support_plan import station_choices
from wien_support_verify import unpack


def replay(context, rows, root, name, note):
    path = root / (name + ".npz")
    if digest(path) != note["mesh_sha256"]:
        raise ValueError("Saved whole support bytes changed")
    candidates, short, deferred = station_choices(
        [{**r, "original": {"bridge": True}} for r in rows], selection="uniform"
    )
    if (
        len(candidates) != len(note["stations"])
        or short != note["short_sources"]
        or deferred != note["deferred_stations"]
    ):
        raise ValueError("Whole original station/deferred ledger differs")
    attempts = sum(len(row["trials"]) for row in note["stations"])
    if (
        attempts != note["work"]["attempts"]
        or attempts > 640
        or note["work"]["builder_calls"] > attempts
        or note["accepted"] > 128
    ):
        raise ValueError("Saved bounded station work/ownership counts differ")
    bodies, caps, _data = unpack(path)
    previous, used, work = [[], []], 0, {"new_pier_pair_checks": 0}
    for candidate, saved in zip(candidates, note["stations"], strict=True):
        owner, line, nominal = candidate
        if (saved["source_id"], saved["nominal_m"]) != (
            rows[owner]["source_id"],
            nominal,
        ):
            raise ValueError("Whole original support station identity changed")
        trials = [(r["shift_m"], r["lateral_m"]) for r in saved["trials"]]
        if trials != list(VARIANTS[name][: len(trials)]):
            raise ValueError("Bounded station candidate order differs")
        if "product" not in saved:
            if not saved.get("reason"):
                raise ValueError("Omitted original station lacks a reason")
            continue
        if (
            saved["product"] != used
            or not trials
            or trials[-1] != (saved["shift_m"], saved["lateral_m"])
            or saved["reason"]
            or used >= len(bodies)
        ):
            raise ValueError("Saved support ownership/selected trial differs")
        product = SupportVolume(bodies[used], *caps[used])
        expected = footprint(line, nominal + saved["shift_m"], saved["lateral_m"])
        actual, physical = admit_station(context, product, expected, previous, work)
        if physical != saved["physical"]:
            raise ValueError("Saved actual support contact proof differs")
        for pool, value in zip(previous, actual, strict=True):
            pool.append(value)
        used += 1
    if (
        used != len(bodies)
        or used != note["accepted"]
        or spacing(rows, note["stations"]) != note["source_spacing"]
    ):
        raise ValueError("Saved whole support/spans ledger incomplete")
    return {
        "accepted": used,
        "independent_new_pier_pair_checks": work["new_pier_pair_checks"],
        "complete_recipe_spans": all(
            s["within_recipe_span"] for s in note["source_spacing"]
        ),
    }


def verify(args):
    report = json.loads((args.output / "report.json").read_text())
    context, rows, base = load(args.deck, args.fixture, construction=False)
    if (
        report["deck_report_sha256"] != digest(args.deck / "report.json")
        or report["deck_candidates_sha256"] != base["candidates_sha256"]
        or report["fixture_sha256"] != digest(args.fixture)
        or report["old_road_arrays"] != base["old_road_arrays"]
        or report["old_source_ids"] != base["old_source_ids"]
        or report["rail_candidate_source_ids"] != base["source_ids"]
        or report["origin_lat_lon"] != base["origin_lat_lon"]
        or report["admitted_source_ids"]
    ):
        raise ValueError(
            "Exact deck/whole world/source/frame/non-admission binding changed"
        )
    result = {
        name: replay(context, rows, args.output, name, note)
        for name, note in report["methods"].items()
    }
    selected = choose(report["methods"])
    if (
        selected != report["selected"]
        or result[selected]["complete_recipe_spans"] != report["complete_recipe_spans"]
    ):
        raise ValueError("Whole-support selection changed")
    proof = {
        "report_sha256": digest(args.output / "report.json"),
        "selected": selected,
        "methods": result,
        "valid_whole_support_candidate": result[selected]["accepted"] > 0,
        "valid_retained_deck": True,
        "scope": "Actual body/source station/whole contact/traffic/old-new pier/reason/span re-read. No support or deck reconstruction, native/world/structural/consumer admission.",
    }
    (args.output / "verification.json").write_text(json.dumps(proof, indent=2) + "\n")
    print(
        json.dumps(
            {
                "selected": selected,
                "accepted": result[selected]["accepted"],
                "complete_recipe_spans": result[selected]["complete_recipe_spans"],
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--deck", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, default=DEFAULT_FIXTURE)
    parser.add_argument("--output", type=Path, required=True)
    verify(parser.parse_args())
