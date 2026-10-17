"""Whole and explicitly clipped views of the three actual closed open-floor parts."""

import argparse
from dataclasses import asdict
import json
from pathlib import Path

import numpy as np

from evidence import revision
from industrial_relief_probe import camera
from outshine_ref.buildings.rooflight_panels import triangle_surface
from podium_probe import write
from stepped_source_probe import overview
from structural_shell_geometry import native
from terraced_mass_collision import ComponentColumns
from terraced_mass_plan import plan
from terraced_mass_rooms import build
from terraced_mass_sources import sources


def draw(asset, core, clipped):
    meshes = []
    normal = np.r_[core.across_xy, 0.0]
    offset = (
        np.asarray(core.origin_xy) @ np.asarray(core.across_xy) + core.size_m[0] / 2
    )
    for part, material in ((asset.shell, 12), (asset.stairs, 5), (asset.landing, 4)):
        raw = part.mesh
        if clipped:
            solid = native(raw, part.vertex_roots).trim_by_plane(normal, offset)
            data = solid.to_mesh64()
            raw = {
                "vertices": np.asarray(data.vert_properties)[:, :3],
                "faces": np.asarray(data.tri_verts),
            }
        if len(raw["faces"]):
            meshes.append(triangle_surface(raw, material))
    starts = np.cumsum([0] + [len(m["vertices"]) for m in meshes[:-1]])
    return {
        key: np.concatenate(
            [
                m[key] + start if key == "faces" else m[key]
                for m, start in zip(meshes, starts, strict=True)
            ]
        )
        for key in meshes[0]
    }


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError(
            "Freeze the complete open-floor prototype before image evidence"
        )
    output.mkdir(parents=True, exist_ok=True)
    row = sources()[0]
    for kind in ("cascade", "paired-wings"):
        p = plan(row, kind)
        asset = build(p)
        # Exercise the actual closed component decoders before rendering.
        ComponentColumns(asset, -1, p.top_m + 1)
        direction = np.r_[
            -np.asarray(p.core.across_xy) - 0.7 * np.asarray(p.core.along_xy), 0.8
        ]
        points = np.concatenate(
            [
                part.mesh["vertices"]
                for part in (asset.shell, asset.stairs, asset.landing)
            ]
        )
        view = overview(points, direction)
        for clipped in (False, True):
            name = kind + ("Cutaway" if clipped else "Whole")
            write(
                output,
                name,
                draw(asset, p.core, clipped),
                {
                    "camera": camera(view),
                    "source_input": {
                        "id": row["id"],
                        "properties": row["properties"],
                        "footprint_wkb": row["polygon"].wkb.hex(),
                    },
                    "mass_plan": json.loads(
                        json.dumps(asdict(p), default=lambda v: v.hex())
                    ),
                    "clipped": clipped,
                    "scope": "Three closed actual components, no final Boolean union. Cutaway is an explicitly clipped/capped diagnostic, not a complete asset. Simple open floors and actual stairs; no windows, terrace portals, facade programme, rails, native or whole-city admission.",
                },
                proof,
            )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == Path("/tmp") or not output.is_relative_to(Path("/tmp")):
        parser.error("--output requires a subdirectory of /tmp")
    models(output)


if __name__ == "__main__":
    main()
