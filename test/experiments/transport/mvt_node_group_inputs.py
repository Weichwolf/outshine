"""Read the actual #1486 CSR/digest fields; never qualify an end connection.

SourceConnectionCount means original outgoing records. It is not a count of
physical incident members. The full source/member/profile export remains separate.
"""

import hashlib
import struct
from collections.abc import Mapping
from dataclasses import dataclass
from itertools import pairwise

from outshine_ref.transport.canonical_ports import PortBudget, PortSource


@dataclass(frozen=True, slots=True)
class NodeGroupRow:
    first_node: int
    node_count: int
    source_connection_count: int


@dataclass(frozen=True, slots=True)
class NodeGroupPartition:
    groups: tuple[NodeGroupRow, ...]
    nodes: tuple[int, ...]
    digest: bytes
    network_revision: str

    def nodes_of(self, group):
        return self.nodes[group.first_node : group.first_node + group.node_count]

    @property
    def complete_original_incidence(self):
        return False


def unsigned(value, bits, *, positive=False):
    return type(value) is int and int(positive) <= value < 2**bits


def unpack_groups(source, payload, budget, work):
    raw_groups, raw_nodes, raw_digest = (
        payload.get(key) for key in ("Groups", "Nodes", "Digest")
    )
    if not isinstance(raw_groups, (tuple, list)) or not isinstance(
        raw_nodes, (tuple, list)
    ):
        return None, "incomplete_node_group_payload"
    if (
        len(raw_groups) > budget.input_nodes
        or len(raw_nodes) > budget.input_nodes
        or len(source.nodes) > budget.input_nodes
    ):
        return None, "node_group_input_budget"
    if not isinstance(raw_digest, (bytes, tuple, list)) or len(raw_digest) != 32:
        return None, "invalid_node_group_digest"
    if not all(unsigned(value, 8) for value in raw_digest):
        return None, "invalid_node_group_digest"
    digest = bytes(raw_digest)
    positions = {node.identifier: node.longitude_latitude for node in source.nodes}
    if len(positions) != len(source.nodes):
        return None, "duplicate_source_node"
    work["validated_source_nodes"] = len(positions)
    if any(not unsigned(node, 64, positive=True) for node in raw_nodes):
        return None, "invalid_partition_node"
    if (
        len(raw_nodes) != len(positions)
        or len(set(raw_nodes)) != len(raw_nodes)
        or set(raw_nodes) != positions.keys()
    ):
        return None, "incomplete_published_node_partition"
    rows, offset, previous_minimum = [], 0, 0
    for raw in raw_groups:
        if not isinstance(raw, Mapping):
            return None, "invalid_node_group_record"
        first, count, outgoing = (
            raw.get(key) for key in ("FirstNode", "NodeCount", "SourceConnectionCount")
        )
        if (
            not unsigned(first, 32)
            or not unsigned(count, 32, positive=True)
            or not unsigned(outgoing, 64)
        ):
            return None, "invalid_node_group_record"
        if first != offset or count > len(raw_nodes) - offset:
            return None, "invalid_node_group_csr_range"
        ids = raw_nodes[first : first + count]
        if ids[0] <= previous_minimum or any(a >= b for a, b in pairwise(ids)):
            return None, "noncanonical_node_group_order"
        anchor = positions[ids[0]]
        if any(positions[node] != anchor for node in ids):
            return None, "node_group_position_mismatch"
        rows.append(NodeGroupRow(first, count, outgoing))
        previous_minimum = ids[0]
        offset += count
        work["validated_groups"] += 1
    if offset != len(raw_nodes):
        return None, "incomplete_node_group_csr_ranges"
    return (tuple(rows), tuple(raw_nodes), digest), ""


def read_mvt_node_groups(source, capture_state, payload, *, budget=None):
    """Decode native header values, not a new JSON wrapper or capture schema.

    State is the exact uint8 enum:0 NotRequested,1 Captured,2 BudgetExceeded.
    Missing/budget/invalid data returns a reason before any geometry/DEM work.
    A returned partition retains no proof of complete original physical incidence.
    """
    if budget is None:
        budget = PortBudget()
    if not isinstance(source, PortSource) or not isinstance(budget, PortBudget):
        raise TypeError("existing PortSource/PortBudget required")
    work = {
        "validated_source_nodes": 0,
        "validated_groups": 0,
        "digest_node_words": 0,
        "expanded_pairs": 0,
    }
    if capture_state is None:
        return None, "missing_node_group_capture_state", work
    if type(capture_state) is not int or capture_state not in (0, 1, 2):
        return None, "invalid_node_group_capture_state", work
    if capture_state != 1:
        if payload is not None:
            return None, "node_group_state_payload_mismatch", work
        reason = (
            "node_groups_not_requested"
            if capture_state == 0
            else "node_group_capture_budget_exceeded"
        )
        return None, reason, work
    if not isinstance(payload, Mapping):
        return None, "missing_captured_node_groups", work
    identity = source.binding.get("source_identity", {})
    if not isinstance(identity, Mapping):
        return None, "node_group_source_identity_required", work
    revision = identity.get("Revision")
    if (
        identity.get("DatasetId") != "outshine/mvt/transport"
        or not isinstance(revision, str)
        or len(revision) != 64
        or any(c not in "0123456789abcdef" for c in revision)
    ):
        return None, "node_group_source_identity_required", work
    decoded, reason = unpack_groups(source, payload, budget, work)
    if reason:
        return None, reason, work
    rows, nodes, digest = decoded
    # Exact CanonicalizeNodeGroups wire sequence; FirstNode is packed, not hashed.
    hasher = hashlib.sha256(b"outshine/mvt-node-groups/1" + revision.encode("ascii"))
    hasher.update(struct.pack("<QQ", len(rows), len(nodes)))
    for row in rows:
        hasher.update(struct.pack("<IQ", row.node_count, row.source_connection_count))
        for node in nodes[row.first_node : row.first_node + row.node_count]:
            hasher.update(struct.pack("<Q", node))
            work["digest_node_words"] += 1
    if hasher.digest() != digest:
        return None, "node_group_digest_mismatch", work
    return NodeGroupPartition(rows, nodes, digest, revision), "", work
