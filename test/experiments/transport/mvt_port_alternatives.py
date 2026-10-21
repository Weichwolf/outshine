"""Finite observed-port search; every accepted replacement passes the same qualifier."""

import itertools
import math
import time

from compare_mvt_profile_windows import qualify
from mvt_native_cut_profiles import cut_profiles
from mvt_profile_windows import select_window


def _groups(base, profiles):
    groups = []
    for window in base["windows"]:
        if window["role"] == "whole_changed_source":
            continue
        key = window["source_id"]
        low, high = window["requested_range_m"]
        rows = [p for p in profiles if p["source_id"] == key]
        groups.extend(
            (
                sorted(
                    (p for p in rows if p["source_station_m"] <= low),
                    key=lambda p: -p["source_station_m"],
                ),
                sorted(
                    (p for p in rows if p["source_station_m"] >= high),
                    key=lambda p: p["source_station_m"],
                ),
            )
        )
    return groups


def _span(groups, indices):
    stations = [g[i]["source_station_m"] for g, i in zip(groups, indices, strict=True)]
    return math.fsum(b - a for a, b in zip(stations[::2], stations[1::2], strict=True))


def search_ports(
    case,
    native,
    proposal,
    seed,
    *,
    padding_m=2,
    strategy="outer_then_shrink",
    max_candidates=128,
    max_fits=16,
    max_sources=8,
    max_span_m=128,
    qualifier=qualify,
):
    """Return the last fully qualified body, or no replacement under a finite cap.

    span_order tries the finite catalog by increasing total target-axis span.
    outer_then_shrink first qualifies the widest observed combination, then tries
    inward alternatives for each boundary, keeping every other boundary fixed.
    If the outer combination fails, it falls back to span_order. Neither method
    infers monotonic feasibility; one-pass shrinking is a heuristic, not an optimum.
    Time limits inside the unchanged qualifier remain cooperative per-fit limits.
    """
    if strategy not in ("span_order", "outer_then_shrink") or any(
        type(n) is not int or not 1 <= n <= cap
        for n, cap in ((max_candidates, 4096), (max_fits, 256))
    ):
        raise ValueError("known strategy and finite candidate/fit caps required")
    if not 1 <= len(native["cuts"]) <= 64:
        raise ValueError("bounded native port catalog required")
    began = time.perf_counter()
    profiles = cut_profiles(native)
    options = {
        "padding_m": padding_m,
        "max_sources": max_sources,
        "max_span_m": max_span_m,
    }
    base = select_window(case, proposal, seed, interfaces=profiles, **options)
    report = {
        "strategy": strategy,
        "valid": False,
        "max_candidates": max_candidates,
        "max_fits": max_fits,
        "candidates_examined": 0,
        "fit_calls": 0,
        "duplicate_candidates_skipped": 0,
        "attempts": [],
        "initial_selection": base,
        "scope": "Finite geometric catalog only; source-face ownership, outside packing, native nav/terrain and world admission remain open. Shrinking is not a global or catalog optimality proof.",
    }
    if not base["windows"] or base.get("profile_requests"):
        report.update(
            reason="boundary_contract_or_budget", elapsed_s=time.perf_counter() - began
        )
        return None, report
    groups = _groups(base, profiles)
    if not groups or any(not g for g in groups):
        report.update(
            reason="missing_observed_boundary", elapsed_s=time.perf_counter() - began
        )
        return None, report
    report["catalog_combinations"] = math.prod(map(len, groups))
    report["boundary_options"] = [len(g) for g in groups]
    seen, best, products = set(), None, None
    exhausted = False

    def attempt(indices):
        nonlocal products, best, exhausted
        if indices in seen:
            report["duplicate_candidates_skipped"] += 1
            return False
        if (
            report["candidates_examined"] >= max_candidates
            or report["fit_calls"] >= max_fits
        ):
            exhausted = True
            return False
        seen.add(indices)
        report["candidates_examined"] += 1
        ports = [g[i] for g, i in zip(groups, indices, strict=True)]
        selection = select_window(case, proposal, seed, interfaces=ports, **options)
        row = {"indices": list(indices), "selection": selection}
        report["attempts"].append(row)
        if selection["status"] != "captured_candidate":
            row["status"] = "contract_or_budget_declined"
            return False
        report["fit_calls"] += 1
        start = time.perf_counter()
        try:
            candidate, proof = qualifier(case, native, selection)
        except ValueError as error:
            candidate, proof = None, {"valid": False, "reason": str(error)}
        row.update(proof=proof, qualifier_s=time.perf_counter() - start)
        accepted = proof["valid"] and candidate is not None
        row["status"] = "qualified" if accepted else "fit_rejected"
        if accepted:
            best, products = indices, candidate
            report.update(valid=True, selected=selection)
        return accepted

    def span_search():
        nonlocal exhausted
        # Materialize only an explicitly affordable finite catalog, never a prefix
        # advertised as sorted over a larger unexamined product.
        if report["catalog_combinations"] > max_candidates:
            exhausted = True
            return
        ordered = sorted(
            itertools.product(*(range(len(g)) for g in groups)),
            key=lambda i: (_span(groups, i), i),
        )
        for indices in ordered:
            if attempt(indices) or exhausted:
                return

    if strategy == "span_order":
        span_search()
    else:
        outer = tuple(len(g) - 1 for g in groups)
        if attempt(outer):
            for axis in range(len(groups)):
                for index in range(best[axis]):
                    trial = list(best)
                    trial[axis] = index
                    if attempt(tuple(trial)) or exhausted:
                        break
                if exhausted:
                    break
        elif not exhausted:
            span_search()
    report.update(
        reason="work_budget"
        if exhausted
        else ("qualified" if products is not None else "finite_catalog_exhausted"),
        elapsed_s=time.perf_counter() - began,
    )
    return products, report
