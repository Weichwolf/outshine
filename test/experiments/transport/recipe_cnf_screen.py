"""Size/parameter screen of a real recipe-choice stage, not a SAT solver.

Physical candidate admissibility, pair clearances and complete support spans are
NOT replaced by this partial choice-domain CNF. No new support product is built.
The published counting algorithm is deliberately not implemented or benchmarked.
"""

import argparse
import hashlib
import json
import math
import subprocess
import time
from collections import Counter
from pathlib import Path

from elevated_rail_inputs import read_sources
from experiment_output import create_output
from rail_supports_plan import VARIANTS, footprint
from stair_support_plan import station_choices

HERE = Path(__file__).resolve().parent
RECIPE = HERE / "fixtures/stair-elevated-rail-vienna.json"


def exactly_one(variables, next_variable):
    """A count-preserving 3-CNF; auxiliary OR values are uniquely determined."""
    if (
        not variables
        or any(type(v) is not int or v <= 0 for v in variables)
        or len(set(variables)) != len(variables)
        or type(next_variable) is not int
        or next_variable <= max(variables)
    ):
        raise ValueError(
            "Unique positive recipe variables and fresh auxiliary IDs required"
        )
    clauses = [(-a, -b) for at, a in enumerate(variables) for b in variables[at + 1 :]]
    remaining = list(variables)
    while len(remaining) > 3:
        a, b = remaining[:2]
        auxiliary = next_variable
        next_variable += 1
        clauses.extend(((-a, auxiliary), (-b, auxiliary), (a, b, -auxiliary)))
        remaining = [auxiliary, *remaining[2:]]
    clauses.append(tuple(remaining))
    return clauses, next_variable


def parameters(n, clauses):
    """Necessary published fast-branch checks, never a sufficient crossover."""
    if n < 1 or not clauses:
        raise ValueError("Nonempty finite choice CNF required")
    logarithm = (n - 1).bit_length()
    loglog = max(1, (logarithm - 1).bit_length())
    r = math.isqrt(logarithm) // loglog**2
    q = logarithm // (128 * loglog)
    radius = 4 * (math.isqrt(logarithm) + (math.isqrt(logarithm) ** 2 != logarithm))
    # Appendix D's fixed-density reduction chooses s=n+m, not its wire count.
    size = n + len(clauses)
    tensor_log = (size - 1).bit_length()
    occurrences = Counter(abs(v) for clause in clauses for v in clause)
    literal_ports = sum(map(len, clauses))
    equality_inner_edges = sum(max(0, count - 3) for count in occurrences.values())
    return {
        "interpolation": {
            "lambda": logarithm,
            "nu": loglog,
            "r": r,
            "Q": q,
            "R_beta1": radius,
            "necessary_r_at_least_2": r >= 2,
            "necessary_Q_at_least_6R": q >= 6 * radius,
        },
        "tensor": {
            "s_n_plus_m": size,
            "lambda": tensor_log,
            "epsilon_denominator": 128 * 1024,
            "root_u": tensor_log // (128 * 1024),
            "necessary_root_u_positive": tensor_log >= 128 * 1024,
            "fixed_density_bipartite_wires": 2 * (literal_ports + equality_inner_edges),
            "initial_precision_bits": n + 1,
            "extension_degree": 4 * tensor_log,
            "one_extended_ring_entry_payload_bits": (n + 1) * 4 * tensor_log,
        },
        "scope": "Literal published schedules, main 4.8/6.4 and appendix D.4.2. Failed necessary checks exclude these fast constructions at this size. Passing them alone would not establish correctness, a practical crossover or an 8-GB bound; no optimized SAT implementation is ruled out.",
    }


def choice_instance(recipe_path=RECIPE):
    recipe, _context, _sources, rows = read_sources(recipe_path, ())
    stations, short, deferred = station_choices(
        [{**row, "original": {"bridge": True}} for row in rows], selection="uniform"
    )
    if short or deferred or len(stations) > 128:
        raise ValueError("The complete bounded station request is required")
    choices = VARIANTS["longitudinal"]
    # Every requested station has the five declared shifts plus explicit omission.
    width = len(choices) + 1
    original_variables = width * len(stations)
    auxiliary = original_variables + 1
    clauses, domain_only_forbidden, groups = [], [], []
    for at, (owner, line, nominal) in enumerate(stations):
        variables = tuple(range(at * width + 1, (at + 1) * width + 1))
        local, auxiliary = exactly_one(variables, auxiliary)
        clauses.extend(local)
        rejected = []
        for variable, (shift, lateral) in zip(variables[:-1], choices, strict=True):
            try:
                footprint(line, nominal + shift, lateral)
            except ValueError as failure:
                clauses.append((-variable,))
                domain_only_forbidden.append(variable)
                rejected.append({"shift_m": shift, "reason": str(failure)})
        groups.append(
            {
                "source_id": rows[owner]["source_id"],
                "nominal_m": nominal,
                "choice_variables": variables,
                "omit_variable": variables[-1],
                "source_domain_rejections": rejected,
            }
        )
    return (
        groups,
        clauses,
        auxiliary - 1,
        {
            "recipe_sha256": hashlib.sha256(recipe_path.read_bytes()).hexdigest(),
            "reference_fixture_sha256": recipe["reference_fixture_sha256"],
            "source_ids": recipe["source_ids"],
            "stations": len(stations),
            "physical_candidates_most": len(stations) * len(choices),
            "choice_variables": original_variables,
            "unique_or_auxiliaries": auxiliary - 1 - original_variables,
            "source_domain_forbidden": len(domain_only_forbidden),
            "unknown_physical_contact_or_old_exclusion_predicates_most": len(stations)
            * len(choices),
            "foreign_station_candidate_pairs_most": math.comb(len(stations), 2)
            * len(choices) ** 2,
            "physical_feasibility_known": False,
            "scope": "Actual source-bound station/shift request and valid source station domain only. Contact/both-precision body audits, old traffic, new-pier pairs and source span constraints remain absent. Satisfying assignments do not qualify a physical support layout.",
        },
    )


def run(args):
    sources = [
        Path(__file__),
        HERE / "rail_supports_plan.py",
        HERE / "elevated_rail_inputs.py",
        HERE / "stair_support_plan.py",
    ]
    if subprocess.check_output(
        [
            "git",
            "status",
            "--porcelain",
            "--untracked-files=all",
            "--",
            *map(str, sources),
        ],
        text=True,
    ).strip():
        raise ValueError("Freeze the recipe screen sources first")
    start = time.process_time()
    groups, clauses, count, actual = choice_instance(args.recipe)
    elapsed = time.process_time() - start
    create_output(args.output)
    lines = [
        f"p cnf {count} {len(clauses)}",
        *(" ".join(map(str, clause)) + " 0" for clause in clauses),
    ]
    (args.output / "choice-domain.cnf").write_text("\n".join(lines) + "\n")
    (args.output / "station-choices.json").write_text(
        json.dumps(groups, indent=2) + "\n"
    )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "actual_request": actual,
        "cnf_variables": count,
        "cnf_clauses": len(clauses),
        "literal_occurrences": sum(map(len, clauses)),
        "maximum_clause_width": max(map(len, clauses)),
        "published_parameters": parameters(count, clauses),
        "screen_preparation_cpu_s": elapsed,
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sources
        },
        "scope": "Reproducible reduction-size and parameter analysis only; no counting solver, physical geometry construction, solver runtime/quality comparison, native or device claim.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recipe", type=Path, default=RECIPE)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
