"""Real profile/physical-body operators on the isolated common recipe graph."""

from pathlib import Path

import numpy as np
from outshine_ref.contracts import Mesh
from recipe_graph import Emission, Operator, Prepared
from recipe_graph_data import Files, Key, fields
from recipe_transport_inputs import SOURCE_TYPE, validate_source
from recipe_transport_values import (
    compatible,
    payload_bytes,
    physical_facts,
    sealed,
    spatial,
)
from stair_cycle_contacts import contacts
from stair_junction_audit import audit
from stair_junction_body import build
from stair_junction_layout import layout
from stair_junction_solve import solve
from stair_junction_verify import landing_proof, source_fields
from stair_network_geometry import interactions, vertex_links
from stair_outer_geometry import outer_ends
from surface_solid_checks import audit_closed

ROOT = Key("transport.checked-network", "1")
PROFILE = Key("transport.profile-network", "1")
PLAN_TYPE = "transport.profile-plan/1"
BODY_TYPE = "transport.checked-body/1"
CAPABILITIES = ("transport.profile-solve", "transport.whole-body-audit")


def profile_operator(parameters, inputs):
    fields(parameters, ("method", "width_reach_m"))
    if parameters["method"] != "width" or parameters["width_reach_m"] != 0.75:
        raise ValueError("Pilot retains the admitted whole-width contact recipe")
    source = inputs["source"]
    compatible(source)
    validate_source(source)
    value = source.value
    plan = layout(
        value["rows"],
        parameters["method"],
        width_reach_m=parameters["width_reach_m"],
        junctions=value["nodes"],
        stairs=value["stairs"],
    )
    heights, note = solve(plan)
    if heights is None:
        raise ValueError("Bounded transport profile failed: " + note["status"])
    stable = {
        key: item
        for key, item in note.items()
        if key not in ("seconds", "preparation_seconds", "linear_solver_seconds")
    }
    result = sealed(
        {
            "layout": plan,
            "heights": heights,
            "solver": stable,
            "source_fingerprint": source.fingerprint,
            "outer_profiles_unconnected": outer_ends(value["rows"], value["nodes"]),
        }
    )
    return {
        "plan": Emission(
            result,
            payload_bytes(result),
            spatial(source, "finite-profile-layout-float64-not-body-admission", result),
        )
    }


def checked_body(source, profile, data):
    compatible(source, profile)
    if profile.value["source_fingerprint"] != source.fingerprint:
        raise ValueError("Profile plan belongs to a different source snapshot")
    rows, nodes = source.value["rows"], source.value["nodes"]
    for key, array in data.items():
        if not isinstance(array, np.ndarray) or not np.isfinite(array).all():
            raise ValueError("Actual body requires finite numeric arrays")
        if key in ("body_vertices", "reference_vertices", "levels_m"):
            if array.dtype != np.dtype("float64"):
                raise ValueError("Body binary64 port cannot hide downcast coordinates")
        elif array.dtype.kind not in "iu":
            raise ValueError("Actual body indices/masks must remain integers")
    masks = data["face_sources"]
    if np.any(masks <= 0) or np.any(masks >= 1 << len(rows)):
        raise ValueError("Actual body face has absent/unassigned source ownership")
    plan = profile.value["layout"]
    pairs, overlap = interactions(rows, nodes)
    proof = audit(
        data, plan["domains"], limits=[r["gradient"] for r in rows], crossings=pairs
    )
    arrays = {
        "xy": plan["xy"],
        "heights": profile.value["heights"],
        **{"faces" + str(i): part for i, part in enumerate(plan["parts"])},
    }
    source_envelopes = source_fields(arrays, data, rows, plan["domains"])
    proof["landings"] = landing_proof(
        source_envelopes,
        rows,
        plan["domains"],
        nodes,
        [s[0] for s in source.value["stairs"]],
    )
    proof["protected_profiles"] = contacts(
        data, rows, source.value["protected_profiles"]
    )
    proof["interactions"] = overlap
    proof["vertex_links"] = vertex_links(data["body_vertices"], data["body_faces"])
    quantized = data["body_vertices"].astype(np.float32)
    volume = audit_closed(Mesh(quantized.astype(float), data["body_faces"]))
    if volume <= 0:
        raise ValueError("Actual Float32 body has nonpositive volume")
    proof["float32_volume_m3"] = volume
    return sealed(
        {"data": data, "float32_vertices": quantized, "proof": physical_facts(proof)}
    )


def body_operator(parameters, inputs):
    fields(parameters, ())
    source, profile = inputs["source"], inputs["plan"]
    compatible(source, profile)
    if profile.value["source_fingerprint"] != source.fingerprint:
        raise ValueError("Profile plan belongs to a different source snapshot")
    _body, data, _geometry = build(profile.value["layout"], profile.value["heights"])
    result = checked_body(source, profile, data)
    return {
        "body": Emission(
            result,
            payload_bytes(result),
            spatial(
                source, "body-float64-and-checked-float32-xyz-integer-topology", result
            ),
        )
    }


def operators():
    return (
        Operator(
            Key("transport.solve-profiles", "1"),
            {"source": SOURCE_TYPE},
            {"plan": PLAN_TYPE},
            profile_operator,
        ),
        Operator(
            Key("transport.build-checked-body", "1"),
            {"source": SOURCE_TYPE, "plan": PLAN_TYPE},
            {"body": BODY_TYPE},
            body_operator,
        ),
    )


def prepare(*, budget=None, loader=None):
    recipes = Path(__file__).resolve().parents[1] / "assets/recipes"
    loader = loader or Files(
        {
            ROOT: recipes / "transport-checked-network-v1.json",
            PROFILE: recipes / "transport-profile-network-v1.json",
        }
    )
    return Prepared(ROOT, loader, operators(), capabilities=CAPABILITIES, budget=budget)
