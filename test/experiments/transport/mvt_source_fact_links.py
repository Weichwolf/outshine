"""Cross-check #1488 decoded DTOs against the same normalized source.

This is a structural guard, not the final paired-export loader, a SourceOf
ownership proof, a profile conversion or any metric/end-contact qualification.
"""

from collections.abc import Mapping

from mvt_node_group_inputs import NodeGroupPartition
from mvt_source_fact_digest import native_source_fact_digest, version_from_schema
from outshine_ref.transport.canonical_ports import PortBudget, PortSource


def member_link(member, spans, ways, node_groups):
    edge = member["Edge"]
    identity = edge["PathId"], edge["SegmentOrdinal"]
    physical = member["End"] ^ edge["Direction"]
    span = spans.get(identity)
    if span is None or span.nodes[physical] != member["DomainNode"]:
        return None, "source_member_endpoint_mismatch"
    if node_groups.get(member["DomainNode"]) != member["NodeGroup"]:
        return None, "source_member_partition_mismatch"
    aliases = span.attributes.get("native_directed_aliases", ())
    alias = next((row for row in aliases if row["Id"] == edge), None)
    if (
        alias is None
        or alias["FromNodeId" if member["End"] == 0 else "ToNodeId"]
        != member["DomainNode"]
    ):
        return None, "source_member_directed_alias_mismatch"
    local = span.attributes.get("native_path", {}).get("LocalWayBinding", {})
    way = ways.get(member["StreetWay"])
    if way is None or any(
        local.get(key) != way[key] for key in ("StreetWay", "Feature", "Ring")
    ):
        return None, "source_member_way_binding_mismatch"
    step = span.attributes.get("native_source_reference", {}).get("SegmentOrdinal")
    if (
        type(step) is not int
        or step != member["SourceStep"]
        or step >= way["PointCount"] - 1
    ):
        return None, "source_member_step_mismatch"
    return (identity, physical), ""


def group_links(groups, members, spans, ways, node_groups, work):
    seen, offset = set(), 0
    for index, group in enumerate(groups):
        count, first = group["MemberCount"], group["FirstMember"]
        if (
            (count and first != offset)
            or (not count and first != 0)
            or count > len(members) - offset
        ):
            return "source_fact_group_range_mismatch"
        original = group["OriginalEndpointCount"]
        if group["Complete"] != (original == count) or group[
            "FilteredEndpointCount"
        ] != max(original - count, 0):
            return "source_fact_group_completeness_mismatch"
        previous = None
        for member in members[offset : offset + count]:
            if member["NodeGroup"] != index:
                return "source_fact_group_member_mismatch"
            edge = member["Edge"]
            key = (
                edge["PathId"],
                edge["SegmentOrdinal"],
                edge["Direction"],
                member["End"],
            )
            if previous is not None and key <= previous:
                return "noncanonical_source_member_order"
            previous = key
            linked, reason = member_link(member, spans, ways, node_groups)
            if reason:
                return reason
            identity, physical = linked
            if (identity, physical) in seen:
                return "duplicate_physical_source_endpoint"
            seen.add((identity, physical))
            work["validated_members"] += 1
        offset += count
        work["validated_groups"] += 1
    if offset != len(members) or len(seen) != 2 * len(spans):
        return "incomplete_published_source_endpoints"
    return ""


def check_mvt_source_fact_links(source, partition, payload, *, budget=None):
    """Check decoded actual header fields without inventing an export wrapper.

    Source generation/Way count are digest-bound here, but their independent
    owner/scope comparison belongs to the forthcoming paired capture adapter.
    The supplied partition must come from read_mvt_node_groups for this source.
    No source/profile/Unknown changes, DEM, solver, pair expansion or admission.
    Frontier is an exported native fact, not reconstructed from SourceStep:
    woven fragments can share one original step without sharing its terminal.
    """
    budget = PortBudget() if budget is None else budget
    if (
        not isinstance(source, PortSource)
        or not isinstance(partition, NodeGroupPartition)
        or not isinstance(budget, PortBudget)
    ):
        raise TypeError("existing source, validated partition and budget required")
    work = {
        "indexed_spans": 0,
        "validated_groups": 0,
        "validated_members": 0,
        "expanded_pairs": 0,
    }
    if (
        len(source.nodes) > budget.input_nodes
        or len(source.spans) > budget.input_spans
        or len(partition.groups) > budget.input_nodes
    ):
        return "source_fact_link_budget", work
    revision = source.binding.get("source_identity", {}).get("Revision")
    if partition.network_revision != revision:
        return "source_fact_partition_revision_mismatch", work
    try:
        if not isinstance(payload, Mapping):
            return "invalid_source_fact_link_fields", work
        for key, limit in (
            ("Groups", budget.input_nodes),
            ("Members", budget.incidence_visits),
            ("Ways", budget.input_spans),
        ):
            if not isinstance(payload.get(key), (tuple, list)):
                return "invalid_source_fact_link_fields", work
            if len(payload[key]) > limit:
                return "source_fact_link_budget", work
        digest = native_source_fact_digest(
            revision,
            partition.digest,
            payload,
            member_limit=budget.incidence_visits,
            row_limit=max(budget.input_nodes, budget.input_spans),
            version=version_from_schema(payload["schema"])
            if "schema" in payload
            else 1,
        )
        stored = payload.get("Digest")
        if (
            not isinstance(stored, (bytes, tuple, list))
            or len(stored) != 32
            or any(type(v) is not int or not 0 <= v < 256 for v in stored)
            or bytes(stored) != digest
        ):
            return "source_fact_digest_mismatch", work
        groups, members, rows = (payload[key] for key in ("Groups", "Members", "Ways"))
        if len(groups) != len(partition.groups):
            return "source_fact_partition_group_count_mismatch", work
        ways, previous = {}, -1
        for row in rows:
            way = row["StreetWay"]
            if (
                way <= previous
                or way >= payload["SourceWayCount"]
                or row["PointCount"] < 2
            ):
                return "source_fact_way_table_mismatch", work
            ways[way], previous = row, way
        spans = {span.identifier: span for span in source.spans}
        if len(spans) != len(source.spans):
            return "duplicate_physical_source_span", work
        used = {
            span.attributes.get("native_path", {})
            .get("LocalWayBinding", {})
            .get("StreetWay")
            for span in spans.values()
        }
        if used != ways.keys():
            return "source_fact_way_table_mismatch", work
        node_groups = {
            node: index
            for index, group in enumerate(partition.groups)
            for node in partition.nodes_of(group)
        }
        work["indexed_spans"] = len(spans)
        return group_links(groups, members, spans, ways, node_groups, work), work
    except (KeyError, TypeError, ValueError, OverflowError):
        return "invalid_source_fact_link_fields", work
