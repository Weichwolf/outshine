"""Explicit hall-v2 domain adapters; logical glazing precedes all mesh construction."""

from pathlib import Path

import numpy as np
from industrial_hall_shell import structure
from industrial_roof_choice import section_for
from industrial_rooflights import cuts, plan
from outshine_ref.buildings.rooflight_panels import closed
from recipe_graph import Emission, Operator, Prepared
from recipe_graph_data import Files, Key, canonical, fields
from recipe_hall_operators import (
    CAPABILITIES,
    ROOF,
    TYPES,
    HallShell,
    array_bytes,
    compatible,
    representation,
    sealed,
    spatial,
    wall_material,
)
from recipe_hall_operators import (
    operators as previous_operators,
)

ROOT = Key("buildings.industrial-hall", "2")
GLAZING = "buildings.rooflight-plan/1"
PANELS = "buildings.rooflight-panels/1"
SHELL = "buildings.hall-shell/2"
CAPABILITY = "buildings.glazed-hall"


def logical_operator(parameters, inputs):
    fields(parameters, ("max_fields",))
    source, choice, programme = (inputs[k] for k in ("source", "choice", "programme"))
    compatible(source, choice, programme)
    value = plan(
        programme.value.plan,
        section_for(source.value, choice.value),
        max_fields=parameters["max_fields"],
    )
    return {
        "rooflights": Emission(
            value,
            len(canonical(value)),
            spatial(source, "source-owned-rooflight-rectangles-binary64"),
        )
    }


def glazed_operator(parameters, inputs):
    material = wall_material(parameters)
    envelope, programme, glazing = (
        inputs[k] for k in ("envelope", "programme", "rooflights")
    )
    compatible(envelope, programme, glazing)
    body, shell = structure(
        programme.value.plan,
        envelope.value,
        material,
        extra_cuts=cuts(glazing.value, material),
    )
    representation(body, shell.vertex_roots)
    value = HallShell(sealed(body), shell)
    return {
        "shell": Emission(
            value,
            array_bytes(value),
            spatial(envelope, "glazed-hall-shell-float64-topology-roots"),
        )
    }


def panel_representation(parts, count):
    if len(parts) != count:
        raise ValueError("Rooflight panel ownership count disagrees with its plan")
    for part in parts:
        expected = {
            "vertices": ("float64", (132, 3)),
            "normals": ("float64", (132, 3)),
            "uv": ("float64", (132, 2)),
            "facade_extent": ("float64", (132, 3)),
            "faces": ("uint32", (44, 3)),
            "materials": ("uint16", (44,)),
        }
        if set(part) != set(expected) or any(
            not isinstance(part[k], np.ndarray)
            or part[k].dtype != np.dtype(dtype)
            or part[k].shape != shape
            for k, (dtype, shape) in expected.items()
        ):
            raise ValueError("Direct closed rooflight panel representation changed")
        if any(not np.isfinite(part[k]).all() for k in ("vertices", "normals", "uv")):
            raise ValueError("Direct rooflight panel contains nonfinite arrays")


def panel_operator(parameters, inputs):
    fields(parameters, ())
    glazing = inputs["rooflights"]
    compatible(glazing)
    value = closed(glazing.value)
    panel_representation(value, len(glazing.value))
    value = sealed(value)
    return {
        "panels": Emission(
            value,
            array_bytes(value),
            spatial(glazing, "closed-rooflight-panels-float64-uint32"),
        )
    }


def operators():
    return [
        *previous_operators(),
        Operator(
            Key("buildings.rooflight-plan", "1"),
            {
                "source": TYPES["source"],
                "choice": TYPES["roof-choice"],
                "programme": TYPES["hall-programme"],
            },
            {"rooflights": GLAZING},
            logical_operator,
        ),
        Operator(
            Key("buildings.cut-glazed-hall", "1"),
            {
                "envelope": TYPES["roof-envelope"],
                "programme": TYPES["hall-programme"],
                "rooflights": GLAZING,
            },
            {"shell": SHELL},
            glazed_operator,
        ),
        Operator(
            Key("buildings.closed-rooflights", "1"),
            {"rooflights": GLAZING},
            {"panels": PANELS},
            panel_operator,
        ),
    ]


def prepare(*, budget=None, loader=None):
    folder = Path(__file__).parents[1] / "assets" / "recipes"
    files = Files(
        {
            ROOF: folder / "buildings-industrial-envelope-v1.json",
            ROOT: folder / "buildings-industrial-hall-v2.json",
        }
    )
    return Prepared(
        ROOT, loader or files, operators(), (*CAPABILITIES, CAPABILITY), budget
    )
