"""Replay both actual rail components, retaining every original and refusal."""

import argparse
import json
import subprocess
import time
from pathlib import Path
from types import SimpleNamespace

import numpy as np
from ground_contact_plot import draw
from rail_ground_bed import construct
from rail_ground_checks import audit
from rail_ground_inputs import load
from rail_ground_profile import bind_profile
from replay_canonical_ports import serializable


def arrays(result):
    source, plane = result["source"], result["plane"]
    sections = []
    for _identity, band, direction in source.bindings:
        values = result["sections"][band]
        if direction == -1:
            values = values[::-1, ::-1]
        z = plane.evaluate(values.reshape(-1, 2)).reshape(2, 2)
        sections.append(np.concatenate((values, z[:, :, None]), axis=2))
    return {
        "bed_vertices": result["bed"].vertices,
        "bed_triangles": result["bed"].triangles,
        "terrain_vertices": result["terrain"]["mesh"].vertices,
        "terrain_triangles": result["terrain"]["mesh"].triangles,
        "source_members": np.array(
            [m.identifier for m in source.members], dtype=np.uint64
        ),
        "source_nodes": np.array(source.node_ids, dtype=np.uint64),
        "source_axes_xy": result["axes"],
        "source_band_directions": np.array(
            [(band, direction) for _, band, direction in source.bindings],
            dtype=np.int32,
        ),
        "constructed_end_sections_xyz": np.asarray(sections),
        "original_maximum_gradient_words": np.array(
            [
                m.attributes["recipe"]["native_maximum_gradient_word"]
                for m in source.members
            ],
            dtype=np.uint32,
        ),
        "plane": np.array([*plane.origin, plane.intercept, *plane.gradient]),
        "original_dem_observations": result["observations"],
        "terrain_bed_vertex_map": result["terrain"]["road_vertex_map"],
        "terrain_outer_vertex_ids": result["terrain"]["outer_vertex_ids"],
        "terrain_original_prior_z": result["terrain"]["prior_z"],
    }


def replay(args):
    repository = Path(__file__).resolve().parents[3]
    if args.output.resolve().is_relative_to(repository):
        raise ValueError("Private rail outputs belong in system temp")
    args.output.mkdir(parents=True, exist_ok=False)
    index, cases, terrain, budget, metadata, raw, manifest = load()
    report = {
        "schema": "outshine/constructive-rail-ground-replay/1",
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "shared": args.shared,
        "input_manifest": manifest,
        "source_binding": {
            k: metadata["source_binding"][k]
            for k in ("source_identity", "source_facts_digest")
        },
        "cases": [],
        "section_order": "Original source from/to endpoints; right/left while looking along original source direction. Constructed profiles, not native EndContacts.",
        "scope": "Constructed bed and actual DEM earthworks. Original profiles, Unknown, Source/logical routing/EndContact/World/device authority unchanged. No old prior or terrain/geometry budget raised.",
    }
    for case in cases:
        row = {
            "case": case["key"],
            "original_members": case["selected"],
            "original_unknown_nodes": case["original_unknown_nodes"],
            "original_closure_reason": case["closure_reason"],
        }
        began = time.process_time()
        try:
            profile = bind_profile(
                index, case["selected"], metadata["source_binding"], raw
            )
            row["construction_profile"] = profile.record()
            result = construct(index, profile, terrain, budget, shared=args.shared)
            row.update(state=result["state"], reason=result["reason"])
            if result["state"] == "constructed":
                row.update(
                    audit=audit(result, index, profile),
                    work=result["work"],
                    fitting=result["fitting"],
                    original_affine_prior_conflict=result[
                        "original_affine_prior_conflict"
                    ],
                    terrain_candidates=result["terrain"]["candidates"],
                    terrain_refinement=result["terrain"]["refinement"],
                )
                exported = arrays(result)
                np.savez_compressed(args.output / (case["key"] + ".npz"), **exported)
                row["array_fields"] = {
                    k: {"dtype": str(v.dtype), "shape": v.shape, "bytes": v.nbytes}
                    for k, v in exported.items()
                }
                if args.plot:
                    directory = args.output / case["key"]
                    directory.mkdir()
                    draw(
                        SimpleNamespace(
                            road=result["bed"], report={"terminal_values": []}
                        ),
                        result["terrain"],
                        directory,
                        title=f"Constructed rail bed: {len(profile.members)} original sources / {len(result['source'].bands)} physical bands / logical graph unchanged\nActual DEM earthworks; original profiles and Source/World/EndContact remain unqualified",
                    )
        except ValueError as error:
            row.update(state="pending", reason=str(error))
        row["process_cpu_s_with_audit_export_plot"] = time.process_time() - began
        report["cases"].append(row)
        print(json.dumps({k: row[k] for k in ("case", "state", "reason")}), flush=True)
    (args.output / "report.json").write_text(
        json.dumps(serializable(report), indent=2) + "\n"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--shared", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--plot", action="store_true")
    replay(parser.parse_args())


if __name__ == "__main__":
    main()
