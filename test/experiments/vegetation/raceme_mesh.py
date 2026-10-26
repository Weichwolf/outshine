"""Static flowering phase products from a saved recipe, no stochastic meshing."""

import numpy as np
from biome_models import Assembly
from flower_bud import bud_mesh
from outshine_ref.vegetation import Bellflower
from outshine_ref.vegetation.axis_surface import axis_mesh
from outshine_ref.vegetation.curve_frame import arc_frame
from outshine_ref.vegetation.shoot_plan import normal_frame
from raceme_phase import states
from understory_plan import lily_mesh


def pedicel(node, end, growth, segments=8):
    bend = 0.3 + 0.8 * growth
    unit = arc_frame(1, bend, 0, np.array([1.0]))[0][0]
    delta = end - node
    length = np.linalg.norm(delta) / np.linalg.norm(unit)
    local = normal_frame(unit, (1, 0, 0))
    target = normal_frame(delta, (0, 1, 0))
    frame = target @ local.T
    mesh = axis_mesh(length, bend, 0.00025, 0.00012, segments, 6)
    return mesh, frame, length, bend


def phase_mesh(
    recipe,
    phase,
    leaf_segments=24,
    bell_segments=12,
    bell_sides=48,
    selected=None,
    max_organs=64,
    *,
    bud_segments=8,
    bud_sides=16,
):
    state = states(recipe, phase)
    base_ids = {(0, 0), (0, 1), (1, 0)}
    known = base_ids | {
        (kind, i) for kind in (2, 3) for i in range(len(state["active"]))
    }
    requested = known if selected is None else set(selected)
    if not requested <= known or type(max_organs) is not int or max_organs < 0:
        raise ValueError("known organs and nonnegative budget required")
    active = base_ids | {
        (kind, i) for kind in (2, 3) for i in np.flatnonzero(state["active"])
    }
    requested = requested & active
    if not requested or len(requested) > max_organs:
        return None
    assembly, ranges = Assembly(), []
    if requested & base_ids:
        mesh, ranges, _ = lily_mesh(
            recipe["plant"], leaf_segments=leaf_segments, selected=requested & base_ids
        )
        assembly.parts.append(mesh)
        assembly.vertices = len(mesh["vertices"])

    def add(identity, mesh, origin, frame, surface):
        first = assembly.vertices
        assembly.add(mesh, origin[None], frame[None], surface)
        ranges.append({"id": identity, "vertex_range": [first, assembly.vertices]})

    for i in np.flatnonzero(state["active"]):
        node, end, growth = state["nodes"][i], state["ends"][i], state["growth"][i]
        if (2, i) in requested:
            mesh, frame, _, _ = pedicel(node, end, growth)
            add((2, int(i)), mesh, node, frame, 5)
        if (3, i) in requested:
            scale = recipe["plant"]["stalks"][i]["scale"]
            if state["open"][i]:
                scale *= 0.52 + 0.48 * growth
                mesh = Bellflower(0.0075 * scale, 0.0035 * scale).mesh(
                    bell_segments, bell_sides
                )
                surface = 7
            else:
                mesh = bud_mesh(
                    0.0075 * scale * (0.4 + 0.6 * growth),
                    0.0025 * scale * (0.25 + 0.75 * growth),
                    bud_segments,
                    bud_sides,
                )
                surface = 8
            add((3, int(i)), mesh, end, state["frames"][i], surface)
    product = assembly.finish()
    product["faces"] = product["faces"].astype(np.int32, copy=False)
    return product, ranges, state
