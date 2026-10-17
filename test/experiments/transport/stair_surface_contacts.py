"""Bind bounded procedural contacts to verified asset-local original owners."""

import hashlib

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from stair_shared_paving import paving_node
from stair_source_owners import source_selection
from stair_surface_access import contact_access
from stair_surface_cap import cap_properties


def bind_contacts(state, recipe, sources):
    declarations = recipe.get("surface_contacts", [])
    if not isinstance(declarations, list) or len(declarations) > 4:
        raise ValueError("at most four explicit surface contacts required")
    if not declarations:
        return []
    if (
        recipe.get("junction_mode") != "shared_box"
        or recipe.get("contact_mode") != "profile"
    ):
        raise ValueError("surface contacts require the explicit shared_box family")
    result, pairs, total = [], set(), 0
    for chosen in declarations:
        key, old_key = chosen["source_id"], chosen["cached_source_id"]
        pair = (key, old_key)
        if (
            pair in pairs
            or key not in recipe["source_ids"]
            or old_key not in state["cached_module_rows"]
            or old_key in recipe["source_ids"]
        ):
            raise ValueError("unique new/actual cached source contact required")
        pairs.add(pair)
        access = contact_access(
            chosen, sources[key], sources[old_key], state["cached_module_rows"][old_key]
        )
        paving = (
            paving_node(state, recipe, sources, chosen)
            if chosen["kind"] == "minor_footway_node"
            else {}
        )
        matches = [a for a in state["road_assets"] if old_key in a.source_ids]
        if len(matches) != 1:
            raise ValueError("one verified old source asset required")
        asset = matches[0]
        column = asset.source_ids.index(old_key)
        indices = np.flatnonzero(asset.data["owners"][:, column])
        total += len(indices)
        if not len(indices) or total > 32768:
            raise ValueError("surface contact whole-facet budget exhausted")
        owner = recipe["source_ids"].index(key)
        # New limits come from the unmodified original profile, not the recipe.
        from stair_module_inputs import reference_profiles

        kind = "unclassified" if paving else sources[key]["properties"]["subclass"]
        limit = reference_profiles()[kind]["maxGradient"]
        properties, top, _bottom = cap_properties(asset, indices, limit)
        if (
            not properties["old_ground_faces"]
            or not properties["old_depth_exactly_0_3m"]
            or not all(
                c["isotropic_bound_met"] for c in properties["old_cap_gradient_checks"]
            )
        ):
            raise ValueError("actual old cap ground/depth/gradient incompatible")
        selected = source_selection(
            state, old_key, state["report"]["candidate_source_ids"]
        )
        cap = Mesh(state["road"]["body_vertices"], state["road"]["top_faces"][selected])
        if not np.array_equal(cap.vertices[cap.triangles], top):
            raise ValueError("working contact cap differs from asset-local owner")
        result.append(
            {
                **chosen,
                "owner": owner,
                "cap": cap,
                **paving,
                "binding": {
                    "asset_sha256": asset.identity,
                    "asset_owner_column": column,
                    "asset_top_face_indices": indices.tolist(),
                    "top_xyz_sha256": hashlib.sha256(top.tobytes()).hexdigest(),
                    "access": access,
                    **properties,
                },
            }
        )
    return result


def contact_regions(contacts, original):
    result = []
    for contact in contacts:
        cap = contact["cap"]
        old = shapely.union_all(shapely.polygons(cap.vertices[cap.triangles, :2]))
        mask = old.intersection(original[contact["owner"]])
        if mask.is_empty or not mask.is_valid or mask.area <= 1e-7:
            raise ValueError("declared surface contact lacks actual positive overlap")
        result.append({**contact, "mask": mask})
    return result
