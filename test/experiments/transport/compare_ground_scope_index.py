"""Compare repeated snapshot scans with owned indexes and local scope queries."""

import argparse
import json
import struct
import time
from contextlib import nullcontext
from pathlib import Path

from bridge_source_fact_inputs import load_bridge_source_facts
from outshine_ref.transport.canonical_ports import PortNode, PortSource, PortSpan
from outshine_ref.transport.canonical_ports._end_candidates import (
    CandidateGroup,
    CandidateMember,
    CandidateWay,
)
from outshine_ref.transport.canonical_ports._ground_scope import (
    prepare_ground_field_scope,
)
from outshine_ref.transport.canonical_ports._ground_scope_index import (
    prepare_ground_scope_index,
)
from replay_canonical_bridge_ports import allocations


def disjoint_controls(count):
    """Explicit analytical independent chains; not original/native/World facts."""
    nodes, spans, groups, ways, seeds = [], [], [], [], []
    half_word = struct.unpack("<I", struct.pack("<f", 6))[0]
    grade_word = struct.unpack("<I", struct.pack("<f", 0.045))[0]
    grade = struct.unpack("<f", struct.pack("<I", grade_word))[0]
    binding = {"source_facts_digest": "analytical-disjoint-chain-controls/1"}
    for i in range(count):
        path, a, b = i + 1, 2 * i + 1, 2 * i + 2
        positions = ((0.001 * i, 0.5), (0.001 * i + 0.0005, 0.5))
        nodes.extend((PortNode(a, positions[0]), PortNode(b, positions[1])))
        spans.append(
            PortSpan(
                (path, 0),
                (a, b),
                12,
                grade,
                False,
                False,
                {
                    "native_source_reference": {
                        "Source": "analytical",
                        "SegmentOrdinal": 0,
                    },
                    "native_path": {
                        "LocalWayBinding": {"StreetWay": i, "FeatureContent": (i,)}
                    },
                    "native_directed_aliases": (
                        {
                            "Id": {"PathId": path, "SegmentOrdinal": 0, "Direction": 0},
                            "FromNodeId": a,
                            "ToNodeId": b,
                        },
                    ),
                },
            )
        )
        ways.append(
            CandidateWay(
                i, 2, positions, positions, (True, True), half_word, grade_word
            )
        )
        for end, node in enumerate((a, b)):
            member = CandidateMember((path, 0, 0), end, node, i, True)
            groups.append(
                CandidateGroup(
                    f"analytical-{i}-{end}",
                    positions[end],
                    (member,),
                    1,
                    0,
                    True,
                    False,
                )
            )
        seeds.append((path, 0))
    source = PortSource(
        tuple(nodes),
        tuple(spans),
        (),
        {
            **binding,
            "scope": "Explicit analytical disjoint chains, no native/source/World admission",
            "coverage": ({"WestDeg": -1, "EastDeg": 1, "SouthDeg": 0, "NorthDeg": 1},),
        },
    )
    return source, tuple(groups), tuple(ways), binding, tuple(seeds)


def run_case(inputs, indexed):
    source, groups, ways, binding, seeds = inputs
    build = {"indexed_nodes": 0, "indexed_spans": 0, "source_fact_member_visits": 0}
    prepared = None
    if indexed:
        prepared, reason, build = prepare_ground_scope_index(
            source,
            groups,
            ways,
            source_binding=source.binding,
            candidate_binding=binding,
        )
        if reason:
            raise ValueError(reason)
    signatures, local = [], {}
    for seed in seeds:
        index, selected, bound, reason, work = prepare_ground_field_scope(
            source,
            seed,
            groups,
            ways,
            source_binding=source.binding,
            candidate_binding=binding,
            scope_index=prepared,
        )
        del index
        signatures.append((selected, bound, reason))
        for key, value in work.items():
            local[key] = (
                max(local.get(key, 0), value)
                if key == "queue_peak"
                else local.get(key, 0) + value
            )
    return signatures, {
        "requests": len(seeds),
        "qualified_scopes": sum(not row[2] for row in signatures),
        "index_build": build,
        "request_work": local,
        "numeric_array_copy_bytes": 0,
        "copy_scope": "Index/scope stage creates no numeric arrays. Dictionary/tuple reference traffic and total copied bytes are not instrumented; allocations measured separately.",
    }


def main(args):
    if not 1 <= args.requests <= 128:
        raise ValueError("bounded request count1..128 required")
    args.output.mkdir(parents=True, exist_ok=False)
    source, groups, ways, binding, _ = load_bridge_source_facts(args.input)
    seeds = tuple(
        (8409897393650800292, 0) if i % 2 else (11909413124558356807, 0)
        for i in range(args.requests)
    )
    cases = {
        "actual_repeated_two_span": (source, groups, ways, binding, seeds),
        "analytical_disjoint": disjoint_controls(args.requests),
    }
    results = {}
    for case, inputs in cases.items():
        rows, expected = {}, None
        for name, indexed in (("single_use", False), ("reused", True)):
            trace = args.output / f"{case}-{name}.bin"
            tracking = nullcontext()
            if args.allocation_traces:
                import memray

                tracking = memray.Tracker(str(trace), trace_python_allocators=True)
            start = time.perf_counter()
            with tracking:
                signature, report = run_case(inputs, indexed)
            report["seconds"] = time.perf_counter() - start
            if args.allocation_traces:
                report["allocations"] = allocations(trace)
                report["allocations"]["scope"] = (
                    "Index/scope stage with inputs already decoded; reused includes one index build; "
                    "traced Python allocator; allocation bytes are not copied bytes"
                )
            if expected is None:
                expected = signature
            elif signature != expected:
                raise ValueError(
                    "scope/member/binding/reason changed under index reuse"
                )
            rows[name] = report
        results[case] = rows
    result = {
        "scope": "Scope qualification only, no DEM/solver/field caching/native/device proof; shared Linux host. Reused timings include one whole index construction. Actual case repeats one real component; disjoint chains are explicitly analytical controls.",
        "allocation_tracing": args.allocation_traces,
        "scope_bindings_and_reasons_equal": True,
        "cases": results,
    }
    (args.output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        type=Path,
        default=Path(__file__).parent / "fixtures/bridge-source-facts",
    )
    parser.add_argument("--requests", type=int, default=100)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--allocation-traces", action="store_true")
    main(parser.parse_args())
