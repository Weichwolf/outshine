"""Explicit dispatch between attached profile modules and independent ground seeds."""

import numpy as np
from outshine_ref.contracts import Mesh
from stair_module_audit import audit as attached_audit
from stair_module_geometry import build as attached_build
from stair_module_geometry import old_surfaces
from stair_module_source_contract import verify_source_contract
from stair_seed_audit import audit as seed_audit
from stair_seed_audit import connected
from stair_seed_crossings import crossings_for
from stair_seed_geometry import build as seed_build
from stair_seed_geometry import seed_contract


def surfaces(state, recipe, sources):
    if recipe.get("contact_mode") != "seed":
        return old_surfaces(state, recipe, sources)
    if recipe.get("cached_box_grids_m"):
        raise ValueError("cached source grids cannot belong to an independent seed")
    seed_contract(recipe, sources)
    plan, road = state["plan"], state["road"]
    state["module_contacts"] = []
    ground = Mesh(np.c_[plan["xy"], state["heights"]], plan["faces"][plan["ground"]])
    upper = Mesh(road["body_vertices"], road["bottom_faces"][~state["road_ground"]])
    return {"family": "seed", "recipe": recipe}, None, ground, upper


def build(rows, sources, descriptor, line, ground, sample, **options):
    if descriptor.get("family") == "seed":
        return seed_build(rows, sources, ground, sample, recipe=descriptor["recipe"])
    return attached_build(rows, sources, descriptor, line, ground, sample, **options)


def audit(state, recipe, data, rows, planning, line, upper, *, sources=None):
    if recipe.get("contact_mode") == "seed":
        return seed_audit(state, recipe, data, rows, planning, upper, sources=sources)
    return attached_audit(state, recipe, data, rows, planning, line, upper)


def verify_sources(rows, sources, recipe, contacts, line, nodes, *, cached_grids=None):
    if recipe.get("contact_mode") == "seed":
        seed_contract(recipe, sources)
        connected(recipe, rows, nodes, crossings_for(rows, sources, recipe))
    return verify_source_contract(
        rows, sources, recipe, contacts, line, nodes, cached_grids=cached_grids
    )
