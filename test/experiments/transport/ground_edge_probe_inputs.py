"""Existing bound three-arm/chain roads and the actual whole multi-loop product."""

import time

import numpy as np
from ground_contact_inputs import ContactContext, load_context
from ground_multicycle import generate
from replay_ground_multicycle import check, prepare_inputs


def prepare():
    started = time.process_time()
    contexts = {
        "star": load_context(),
        "chain6": load_context(seed=(246935327359419868, 0), geometry_recipe=None),
    }
    index, cases, terrain, budget, metadata = prepare_inputs()
    case = next(c for c in cases if c["seed"] == (2061326783104604389, 0))
    product = generate(index, case["selected"], terrain, budget)
    audit = check(product)
    context = ContactContext(
        product["mesh"],
        product["anchor"],
        terrain,
        np.empty(0),
        product,
        metadata | {"original_case": case, "road_audit": audit},
        0,
    )
    context.road_dem_z = context.sample_z(context.road.vertices[:, :2])
    contexts["loop25"] = context
    return contexts, time.process_time() - started
