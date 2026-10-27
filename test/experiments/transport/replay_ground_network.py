"""Inspect a real five-span source tree without granting missing topology authority."""

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from ground_network_plot import plot
from ground_network_probe import run
from replay_branched_ground import INPUT
from replay_canonical_ports import serializable

SEED = (259296217502348341, 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--method", choices=("auto", "cone", "lp"), default="auto")
    args = parser.parse_args()
    args.output.mkdir(exist_ok=False, parents=True)
    bundle = load_bridge_source_facts(INPUT)
    manifest = json.loads((INPUT / "manifest.json").read_text())
    terrain, terrain_binding = terrain_input(
        FIXTURE, {"terrain": manifest["terrain_reference"]}
    )
    report, inputs, _field, mesh = run(bundle, terrain, seed=SEED, method=args.method)
    plot(
        args.output,
        inputs,
        mesh,
        report,
        title="Five actual source spans and whole ground sections; topology authority remains Pending",
    )
    np.savez_compressed(
        args.output / "geometry.npz",
        vertices=mesh.vertices,
        faces=mesh.triangles,
        prior_vertices=inputs.surface.vertices,
        prior_faces=inputs.surface.triangles,
        part_roles=inputs.part_roles,
        face_parts=inputs.face_parts,
        port_edges=inputs.port_edges,
        port_parts=inputs.port_parts,
        grade_frames=inputs.grade_frames,
    )
    row = {
        k: v
        for k, v in report.items()
        if k
        not in (
            "source_input",
            "surface",
            "source_prior_surface",
            "source_grade_frames",
        )
    }
    row.update(
        commit=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        working_tree_dirty=bool(
            subprocess.check_output(["git", "status", "--porcelain"])
        ),
        terrain_binding=terrain_binding,
    )
    (args.output / "report.json").write_text(
        json.dumps(serializable(row), default=lambda value: value.tolist(), indent=2)
        + "\n"
    )
    print(
        json.dumps(
            {
                "source_admission_reason": report["source_admission_reason"],
                "source_spans": len(report["source_scope"]["members"]),
                "vertices": len(mesh.vertices),
                "faces": len(mesh.triangles),
                "terminals": len(report["terminals"]["values"]),
                "quality": report["terminals"]["whole_quality"],
            }
        )
    )


if __name__ == "__main__":
    main()
