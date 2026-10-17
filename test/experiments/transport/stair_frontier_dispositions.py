"""Keep physical attempts separate from topology proposals and native admission."""


def source_dispositions(plan, attempts, chosen):
    proposed = {k for p in plan["proposals"] for k in p["source_ids"]}
    accepted = set(attempts[chosen]["source_ids"]) if chosen is not None else set()
    result = {}
    for key in sorted(proposed):
        tested = [a["index"] for a in attempts if key in a["source_ids"]]
        reason = (
            "candidate_in_valid_physical_module"
            if key in accepted
            else "unadmitted_after_failed_joint_recipes_retained_for_later"
            if tested
            else "outside_bounded_geometry_attempts"
        )
        result[key] = {
            "candidate_geometry": key in accepted,
            "attempt_indices": tested,
            "reason": reason,
        }
    return result
