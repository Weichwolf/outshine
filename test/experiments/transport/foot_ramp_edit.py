"""Stage a local border edit; a failed whole proof never publishes half an edit."""

import numpy as np
import shapely
from curb_geometry import audit as audit_border
from foot_ramp_audit import audit as audit_ramp
from foot_ramp_cut import audit as solid_audit
from foot_ramp_cut import cut as solid_cut
from foot_ramp_geometry import FLARE_M, RETURN_M, build
from foot_ramp_joints import joint_error
from retained_surface import surface_domain


def edit(
    row,
    data,
    states,
    source,
    reference,
    limits,
    *,
    upper_reference=None,
    method="contact",
):
    ramp = build(
        row["origin"],
        row["basis"],
        row["triangle"],
        row["half"],
        landing_end=row["landing_end_m"],
        footprint=row["shape"],
    )
    proof = audit_ramp(
        ramp, row, reference, data["traffic"], data["world"], data["obstacles"]
    )
    changed = {}
    joints = []
    queries = 0
    for index in data["tree"].query(row["shape"], predicate="intersects"):
        index = int(index)
        members = states[index]
        staged = []
        touched = False
        for product in members:
            shape = surface_domain(product.lower_contact)
            if not shape.intersects(row["shape"]):
                staged.append(product)
                continue
            touched = True
            if shape.intersection(row["shape"]).area <= 1e-12:
                staged.append(product)
                continue
            if method == "solid":
                new, _note = solid_cut(
                    product, row["shape"], f"transport/border/{index}"
                )
                if new is not None:
                    solid_audit(
                        new, shape.difference(row["shape"]), reference, upper_reference
                    )
                    staged.append(new)
                continue
            if method != "contact":
                raise ValueError("known ramp border edit method required")
            for part in shapely.get_parts(shape.difference(row["shape"])):
                if part.area == 0:
                    continue
                new, note = source.build(part, minimum_height_m=0.1199999)
                if new is None:
                    raise ValueError("ramp_border_rebuild:" + note["reason"])
                audit_border(
                    new,
                    part,
                    reference,
                    data["traffic"],
                    data["world"],
                    data["obstacles"],
                    outside_limit=limits[data["owners"][index]["kind"]],
                )
                staged.append(new)
        if not touched:
            continue
        changed[index] = staged
        before = shapely.union_all([surface_domain(p.lower_contact) for p in members])
        after = shapely.union_all([surface_domain(p.lower_contact) for p in staged])
        if before.difference(row["shape"]).symmetric_difference(after).area > 1e-8:
            raise ValueError("ramp staged border subtraction differs")
        expected_joint = before.difference(row["shape"]).boundary.intersection(
            row["shape"].boundary
        )
        covered = []
        for part in staged:
            line = expected_joint.intersection(
                surface_domain(part.upper_contact).boundary.buffer(1e-8)
            )
            if line.length > 1e-7:
                error, count = joint_error(ramp.upper, part.upper_contact, line)
                joints.append((line, error))
                queries += count
                covered.append(line)
        if (
            expected_joint.difference(shapely.union_all(covered).buffer(1e-8)).length
            > 1e-10
        ):
            raise ValueError(
                "ramp staged border lacks a whole actual planned interface"
            )
    if not changed:
        raise ValueError("ramp has no actual border edit or contact")
    origin, basis = np.array(row["origin"]), np.array(row["basis"])
    width = row["half"] + FLARE_M
    all_joint = shapely.union_all([line for line, _error in joints])
    for x in [-width, width]:
        side = shapely.LineString(
            np.array([[x, -0.01], [x, row["landing_end_m"]]]) @ basis.T + origin
        )
        if side.intersection(all_joint.buffer(1e-8)).length < 1.8:
            raise ValueError("ramp staged sides lack actual required contact length")
    return (
        ramp,
        changed,
        dict(
            proof,
            edited_original_bodies=sorted(changed),
            whole_border_joint_length_m=all_joint.length,
            maximum_border_joint_error_m=max(error for _line, error in joints),
            affine_border_joint_queries=queries,
            core_width_m=2 * row["half"],
            total_ramp_length_m=row["landing_end_m"] + RETURN_M,
        ),
    )
