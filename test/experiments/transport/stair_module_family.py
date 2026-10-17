"""Explicit dispatch between attached profile modules and independent ground seeds."""

import numpy as np
from outshine_ref.contracts import Mesh
from stair_module_audit import audit as attached_audit
from stair_module_geometry import build as attached_build
from stair_module_geometry import old_surfaces
from stair_module_source_contract import verify_source_contract
from stair_seed_audit import audit as seed_audit
from stair_seed_audit import connected
from stair_seed_geometry import build as seed_build
from stair_seed_geometry import seed_contract


def surfaces(state, recipe, sources):
    if recipe.get("contact_mode") != "seed":
        return old_surfaces(state, recipe, sources)
    seed_contract(recipe, sources)
    plan, road = state["plan"], state["road"]
    state["module_contacts"] = []
    ground = Mesh(np.c_[plan["xy"], state["heights"]], plan["faces"][plan["ground"]])
    upper = Mesh(road["body_vertices"], road["bottom_faces"][~state["road_ground"]])
    return {"family": "seed"}, None, ground, upper


def build(rows, sources, descriptor, line, ground, sample, **options):
    if descriptor.get("family") == "seed":
        return seed_build(rows, sources, ground, sample)
    return attached_build(rows, sources, descriptor, line, ground, sample, **options)


def audit(state, recipe, data, rows, planning, line, upper):
    if recipe.get("contact_mode") == "seed":
        return seed_audit(state, recipe, data, rows, planning, upper)
    return attached_audit(state, recipe, data, rows, planning, line, upper)


def verify_sources(rows, sources, recipe, contacts, line, nodes):
    if recipe.get("contact_mode") == "seed":
        seed_contract(recipe, sources)
        connected(recipe, rows, nodes)
    return verify_source_contract(rows, sources, recipe, contacts, line, nodes)
