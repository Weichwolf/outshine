"""Full, omitted, original-depth and projected cap controls on the same real sources."""

import argparse
import json
from dataclasses import replace
from pathlib import Path

import numpy as np
from demand import View
from evidence import revision
from industrial_relief_probe import export
from industrial_relief_products import ArticulatedDetails
from industrial_relief_signal import SignalDetails
from prepared_hall_tile import prepare_halls
from prepared_hall_tile_probe import source_tile


class FullRelief(ArticulatedDetails):
    def draw_parts(self, identifier):
        return tuple(p.outer for p in self.reliefs[identifier])


class ForcedCaps(SignalDetails):
    def draw_parts(self, identifier):
        return self.far_parts[identifier]


def gate_view(plan, *, grazing=False):
    gate = plan["gate"]
    t, centre = np.asarray(gate["tangent"]), np.asarray(gate["centre"])
    normal = np.array([t[1], -t[0]])
    distance, along = (2, 25) if grazing else (18, 8)
    return View(
        np.r_[centre + normal * distance + t * along, 2.6],
        np.r_[centre + t * 2, 2.1],
        width=1920,
        height=1080,
        fov_deg=48,
    )


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze relief signal before evidence")
    rows, receipt, _context, original = source_tile()
    prepared = prepare_halls(rows)
    output.mkdir(parents=True, exist_ok=True)
    results = []
    for label, factor in (("Base", 1), ("Far", 4)):
        view = replace(
            original,
            eye=np.asarray(original.target)
            + factor * (np.asarray(original.eye) - original.target),
        )
        for method, owner in (
            ("Full", FullRelief(prepared, view)),
            ("Omit", ArticulatedDetails(prepared, view)),
            ("Front", SignalDetails(prepared, view)),
            ("Wall", SignalDetails(prepared, view, method="wall")),
        ):
            results.append(
                export(
                    output,
                    "GrohndeSignal" + label + method,
                    owner,
                    prepared.rows,
                    receipt,
                    view,
                    proof,
                )
            )
    identifier = next(iter(prepared.assets))
    for label, grazing in (("Near", False), ("Grazing", True)):
        view = gate_view(prepared.assets[identifier].plan, grazing=grazing)
        for method, owner in (
            ("Full", FullRelief(prepared, view, opened=1, staff_angle=90)),
            ("Auto", SignalDetails(prepared, view, opened=1, staff_angle=90)),
            ("ForcedFront", ForcedCaps(prepared, view, opened=1, staff_angle=90)),
            (
                "ForcedWall",
                ForcedCaps(prepared, view, method="wall", opened=1, staff_angle=90),
            ),
        ):
            results.append(
                export(
                    output,
                    "Signal" + label + method,
                    owner,
                    (owner.source_rows[identifier],),
                    receipt,
                    view,
                    proof,
                )
            )
    (output / "models.json").write_text(
        json.dumps(proof | {"models": results}, indent=2) + "\n"
    )
    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(models(args.output)))
