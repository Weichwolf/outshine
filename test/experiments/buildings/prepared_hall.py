"""Immutable complete hall geometry and stateful reference selection without rebuilding it."""

from dataclasses import dataclass

import numpy as np
from demand import displacement_error
from industrial_hall_details import (
    gate_fixed,
    gate_points,
    personnel_fixed,
    personnel_leaf,
    placed,
    roller_leaf,
    windows,
)
from industrial_hall_shell import ramp
from outshine_ref.buildings.modelling.scope import scalar
from outshine_ref.buildings.roller_gates import lift
from outshine_ref.buildings.rooflight_panels import (
    DEPTH_M,
    MAX_FIELDS,
    closed,
    corners,
    sheets,
)
from recipe_hall_operators import compatible, sealed
from recipe_rooflight_operators import GLAZING, PANELS, SHELL


@dataclass(frozen=True, slots=True)
class HallAsset:
    run: object
    fixed: tuple
    closed_door: object
    closed_gate_full: tuple
    closed_gate_flat: tuple
    full_panels: tuple
    sheet_panels: tuple
    field_bounds: object
    gate_bounds: object

    @property
    def plan(self):
        return self.run.outputs["programme"].value.plan

    @property
    def body(self):
        return self.run.outputs["shell"].value.body


def prepare_asset(run):
    if not {"programme", "shell", "rooflights"} <= set(run.outputs):
        raise ValueError("Prepared hall requires v2 programme, body and rooflights")
    programme, shell, glazing = (
        run.outputs[k] for k in ("programme", "shell", "rooflights")
    )
    compatible(programme, shell, glazing)
    if shell.type_id != SHELL or glazing.type_id != GLAZING:
        raise ValueError("Prepared hall requires v2 physical body and logical glazing")
    fields = glazing.value
    if len(fields) > MAX_FIELDS or len({f["id"] for f in fields}) != len(fields):
        raise ValueError("Prepared hall fields need bounded unique ownership")
    plan = programme.value.plan
    if shell.value.shell.source_ids[0] != plan["id"] + ":roof-envelope" or any(
        not f["id"].startswith(plan["id"] + ":roof-interval-") for f in fields
    ):
        raise ValueError("Prepared hall source ownership disagrees")
    entry = plan["entry"] | {
        "source_footprint_wkb": plan["footprint_wkb"],
        "angle_deg": 0.0,
    }
    gate = plan["gate"]
    frame, housing = gate_fixed(gate["width_m"], gate["height_m"])
    fixed = (
        ramp(plan),
        *personnel_fixed(entry, 1),
        *windows(plan, 1),
        placed(frame, gate),
        placed(housing, gate),
    )
    if "panels" in run.outputs and run.outputs["panels"].type_id != PANELS:
        raise ValueError("Unexpected full panel port type")
    full = run.outputs["panels"].value if "panels" in run.outputs else closed(fields)
    upper = np.stack([corners(f) for f in fields]) if fields else np.empty((0, 4, 3))
    bounds = np.concatenate([upper, upper - [0, 0, DEPTH_M]], axis=1)
    return HallAsset(
        run,
        sealed(fixed),
        sealed(personnel_leaf(entry, 1)),
        sealed(roller_leaf(plan, 0.0, True)),
        sealed(roller_leaf(plan, 0.0, False)),
        sealed(full),
        sealed(sheets(fields)),
        sealed(bounds),
        sealed(gate_points(plan)),
    )


def selection(asset, view):
    r = DEPTH_M / 2
    q = (asset.field_bounds - view.eye) @ view.basis.T
    nearest = q[:, :, 2].min(axis=1) - view.movement_m
    error = np.full(len(q), np.inf)
    valid = nearest > r
    numerator = r * (np.linalg.norm(q, axis=2).max(axis=1) + view.movement_m)
    error[valid] = (
        view.focal_px * numerator[valid] / (nearest[valid] * (nearest[valid] - r))
    )
    return error <= view.detail_error_px, error


@dataclass(frozen=True, slots=True)
class HallDraw:
    body: object
    render_parts: tuple
    physical_parts: tuple
    sheet_mask: object
    gate_detail: bool


class HallInstance:
    def __init__(self, asset):
        self.asset = asset
        self.angle = 0.0
        self.opened = 0.0
        self.door = asset.closed_door
        self.gate_full = asset.closed_gate_full
        self.gate_flat = asset.closed_gate_flat
        self.draw = None
        self.generated_components = 0

    def compose(self, door, gate, roof):
        fixed = self.asset.fixed
        return (fixed[0], door, *fixed[1:], *gate, *roof)

    def update(self, view, *, opened=0.0, staff_angle=0.0):
        opened, angle = scalar(opened), scalar(staff_angle)
        lift(self.asset.plan["definition"], opened)
        if not 0 <= angle <= 90:
            raise ValueError("Personnel angle requires [0,90] degrees")
        mask, _error = selection(self.asset, view)
        detailed = (
            displacement_error(
                self.asset.gate_bounds, view, self.asset.plan["definition"].rib_m
            )
            > view.detail_error_px
        )
        if (
            self.draw is not None
            and opened == self.opened
            and angle == self.angle
            and detailed == self.draw.gate_detail
            and np.array_equal(mask, self.draw.sheet_mask)
        ):
            self.generated_components = 0
            return self.draw
        door, full, flat = self.door, self.gate_full, self.gate_flat
        generated = 0
        if angle != self.angle:
            entry = self.asset.plan["entry"] | {"angle_deg": angle}
            door = sealed(personnel_leaf(entry, 1))
            generated += 1
        if opened != self.opened:
            full = sealed(roller_leaf(self.asset.plan, opened, True))
            flat = sealed(roller_leaf(self.asset.plan, opened, False))
            generated += len(full) + len(flat)
        roof = tuple(
            sheet if use_sheet else complete
            for use_sheet, sheet, complete in zip(
                mask, self.asset.sheet_panels, self.asset.full_panels, strict=True
            )
        )
        render = self.compose(door, full if detailed else flat, roof)
        physical = (
            self.draw.physical_parts
            if self.draw is not None and opened == self.opened and angle == self.angle
            else self.compose(door, full, self.asset.full_panels)
        )
        result = HallDraw(
            self.asset.body, render, physical, sealed(mask), bool(detailed)
        )
        self.angle, self.opened, self.door = angle, opened, door
        self.gate_full, self.gate_flat, self.draw = full, flat, result
        self.generated_components = generated
        return result
