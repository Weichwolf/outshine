"""Conditional actual outside-axis anchors; never a full transverse port claim."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

from mvt_t_connection_inputs import inputs
from mvt_t_profile_layout import prepare_profiles
from mvt_t_profile_region import region_inputs
from mvt_t_profile_route_bound import route_prior_bound
from mvt_t_profile_solve import solve_profiles


def outside_axis_contacts(case, context):
    ways = {f"native-part:{w['native_way']}": w for w in case["ways"]}
    contacts = []
    for group in context["observed_contacts"]:
        if group["status"] != "observed_compatible_native_contact":
            continue
        for outside in group["outside_source_ids"]:
            matches = {
                tuple(row[:3])
                for span in ways[outside]["recorded_profile_spans_xyz_node"]
                for row in span
                if row[:2] == group["xy_m"]
            }
            if len(matches) != 1:
                raise ValueError("one actual recorded outside-axis height required")
            for selected in group["selected_source_ids"]:
                contacts.append(
                    {
                        "contact_id": f"outside-axis:{outside}:{selected}:{group['native_node']}",
                        "source_id": selected,
                        "binding": "fixed",
                        "xyz_m": [list(next(iter(matches)))],
                        "provenance": f"Actual recorded {outside} axis point from native {case['provenance']['native_cpp_commit']}. Conditional unchanged outside asset; a single point, not a whole transverse port.",
                    }
                )
    return contacts


def probe(case, selected_source_ids):
    began = time.perf_counter()
    selected_case, sources, proposal = region_inputs(case, selected_source_ids)
    plan = prepare_profiles(
        selected_case, sources, proposal, mesh_recipe="outward_binary_grid"
    )
    contacts = outside_axis_contacts(case, proposal["region_context"])
    results = []
    for contact in contacts:
        witnesses, unsupported_routes = [], 0
        for junction, owners in enumerate(plan["incident"]):
            keys = [plan["rows"][owner]["source_id"] for owner in owners]
            if contact["source_id"] not in keys:
                continue
            for target in keys:
                if target == contact["source_id"]:
                    continue
                try:
                    witnesses.append(route_prior_bound(plan, contact, junction, target))
                except ValueError as error:
                    if "coverage" not in str(error) and "supported route" not in str(
                        error
                    ):
                        raise
                    unsupported_routes += 1
        algorithms = {}
        for solver in ("highs-ds", "highs-ipm"):
            heights, note = solve_profiles(
                plan, fixed_contacts=[contact], solver=solver
            )
            algorithms[solver] = {
                "produced_heights": heights is not None,
                "solve": note,
            }
        results.append(
            {
                "contact": contact,
                "supported_route_witnesses": witnesses,
                "unsupported_routes": unsupported_routes,
                "algorithms": algorithms,
            }
        )
    return {
        "results": results,
        "elapsed_s": time.perf_counter() - began,
        "scope": "Conditional unchanged actual outside-axis points only; not complete transverse boundary profiles, network impossibility or native admission. The default2m recipe guard is unchanged.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--region-sources", nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    case, _sources = inputs(args.input)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        **probe(case, args.region_sources),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(
        f"{len(report['results'])} conditional outside-axis probes; no boundary admission"
    )


if __name__ == "__main__":
    main()
