"""Matched same-camera near-reference/direct-exterior images and a forced-close counter."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import shapely
from demand import View
from evidence import revision
from industrial_relief_probe import camera
from podium_probe import write
from terraced_facade_prepare import prepare
from terraced_facade_probe import facade_draw, materials
from terraced_facade_products import FacadeProducts
from terraced_mass_plan import plan
from terraced_mass_sources import sources


def export(output, name, mesh, prepared, view, decision, proof, *, forced=False):
    mass = prepared.mass
    return write(
        output,
        name,
        mesh,
        {
            "camera": camera(view),
            "source_id": mass.identifier,
            "source_footprint_wkb": mass.source_wkb.hex(),
            "source": {"material_definitions": materials()},
            "facade_materials": {},
            "ground_span_m": 10000,
            "research_selection": decision,
            "coarse_forced_outside_bound": forced,
            "window_ids_sha256": hashlib.sha256(
                json.dumps([w.identifier for w in prepared.facade.windows]).encode()
            ).hexdigest(),
            "scope": "Same-source/camera/light exterior-position experiment. Full near reference retains all physical floors/access. Direct surface is draw-only with flattened window regions; no radiance/transmission/shadow/temporal/native admission. Forced close is a rejected-detail counter, not normal selection.",
        },
        proof,
    )


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the compact product experiment before image evidence")
    output.mkdir(parents=True, exist_ok=True)
    for index in (0, 3):
        for kind in ("cascade", "paired-wings"):
            p = prepare(plan(sources()[index], kind))
            owner = FacadeProducts(p)
            centre = np.r_[
                shapely.from_wkb(p.mass.source_wkb).centroid.coords[0], p.mass.top_m / 2
            ]
            direction = np.array([1, -1.25, 0.85])
            direction /= np.linalg.norm(direction)
            view = View(centre + direction * 1200, centre, width=1920, height=1080)
            decision = owner.select(view)
            if decision["mode"] != "compact":
                raise ValueError(
                    "Declared far camera exceeds the fixed exterior-position bound"
                )
            near = owner.physical()
            dense = facade_draw(near, p.mass)
            for suffix, mesh in (
                ("NearReference", dense),
                ("Direct", owner.coarse.mesh),
            ):
                export(
                    output, f"{index}-{kind}-{suffix}", mesh, p, view, decision, proof
                )
            if index == 0 and kind == "cascade":
                anchor = np.asarray(centre[:2])
                w = min(
                    (
                        w
                        for w in near.facade.windows
                        if w.storey == 1 and w.edge[0] == 0
                    ),
                    key=lambda w: np.linalg.norm(np.asarray(w.centre) - anchor),
                )
                t, xy = np.asarray(w.tangent), np.asarray(w.centre)
                n = np.array([t[1], -t[0]])
                target = np.r_[xy - 0.10 * n, w.bottom_m + w.height_m / 2]
                close = View(
                    np.r_[xy + 4 * n - 0.8 * t, target[2] + 0.45],
                    target,
                    width=1920,
                    height=1080,
                    fov_deg=42,
                )
                decision = owner.select(close)
                if decision["mode"] != "near":
                    raise ValueError(
                        "Near window camera failed to request the physical product"
                    )
                export(output, "Close-NearReference", dense, p, close, decision, proof)
                export(
                    output,
                    "Close-ForcedDirect",
                    owner.coarse.mesh,
                    p,
                    close,
                    decision,
                    proof,
                    forced=True,
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
