"""Exact #1488 BindSourceFacts byte protocol, independent of its JSON wrapper."""

import hashlib
import math
import struct
from collections.abc import Mapping


def integer(value, bits):
    if type(value) is not int or not 0 <= value < 2**bits:
        raise ValueError(f"actual uint{bits} word required")
    return value


def sequence(value, name):
    if not isinstance(value, (tuple, list)):
        raise TypeError(name + " must be an owned sequence of decoded native fields")
    return value


def record(value):
    if not isinstance(value, Mapping):
        raise TypeError("decoded native record required")
    return value


def boolean(value):
    if type(value) is not bool:
        raise ValueError("actual native bool required")
    return int(value)


def version_from_schema(schema):
    if schema == "outshine/native-mvt-source-facts/1":
        return 1
    if schema == "outshine/native-mvt-source-facts/2":
        return 2
    raise ValueError("known native source fact schema required")


def native_source_fact_digest(
    revision,
    partition_digest,
    payload,
    *,
    member_limit=65536,
    row_limit=16384,
    version=None,
):
    """Hash only actual MvtTransportSourceFacts fields in native order.

    This is integrity binding, not a proof of incidence, matching source/Way
    ownership, closure, profile or contact qualification. Those checks stay in
    the actual paired export adapter. No final export wrapper is assumed here.
    """
    if (
        not isinstance(revision, str)
        or len(revision) != 64
        or any(c not in "0123456789abcdef" for c in revision)
    ):
        raise ValueError("actual normalized Network revision required")
    if not isinstance(partition_digest, bytes) or len(partition_digest) != 32:
        raise ValueError("validated node-partition digest required")
    record(payload)
    declared = version_from_schema(payload["schema"]) if "schema" in payload else None
    if version is None:
        version = 1 if declared is None else declared
    if type(version) is not int or version not in (1, 2):
        raise ValueError("actual native fact protocol version required")
    if declared is not None and declared != version:
        raise ValueError("native schema and byte protocol version disagree")
    groups = sequence(payload.get("Groups"), "Groups")
    members = sequence(payload.get("Members"), "Members")
    ways = sequence(payload.get("Ways"), "Ways")
    if len(groups) > row_limit or len(ways) > row_limit or len(members) > member_limit:
        raise ValueError("source fact input budget")
    hasher = hashlib.sha256(
        b"outshine/mvt-source-facts/" + str(version).encode("ascii")
    )
    hasher.update(revision.encode("ascii"))
    hasher.update(partition_digest)

    def append(fmt, *values):
        hasher.update(struct.pack("<" + fmt, *values))

    append(
        "QQQQQ",
        integer(payload.get("SourceGeneration"), 64),
        integer(payload.get("SourceWayCount"), 64),
        len(groups),
        len(members),
        len(ways),
    )
    for raw in groups:
        group = record(raw)
        append(
            "IIQQBB",
            integer(group.get("FirstMember"), 32),
            integer(group.get("MemberCount"), 32),
            integer(group.get("OriginalEndpointCount"), 64),
            integer(group.get("FilteredEndpointCount"), 64),
            boolean(group.get("Complete")),
            boolean(group.get("Frontier")),
        )
    for raw in members:
        member = record(raw)
        edge = record(member.get("Edge"))
        direction, end = (
            integer(edge.get("Direction"), 8),
            integer(member.get("End"), 8),
        )
        if direction not in (0, 1) or end not in (0, 1):
            raise ValueError("native directed edge/end required")
        append(
            "QIBQIIIB",
            integer(edge.get("PathId"), 64),
            integer(edge.get("SegmentOrdinal"), 32),
            direction,
            integer(member.get("DomainNode"), 64),
            integer(member.get("NodeGroup"), 32),
            integer(member.get("StreetWay"), 32),
            integer(member.get("SourceStep"), 32),
            end,
        )
        if version == 2:
            append("B", boolean(member.get("SourceTerminal")))
    for raw in ways:
        way = record(raw)
        append(
            "IIIIII",
            *(
                integer(way.get(key), 32)
                for key in (
                    "StreetWay",
                    "Feature",
                    "Ring",
                    "PointCount",
                    "HalfWidthWord",
                    "MaximumGradientWord",
                )
            ),
        )
        stencil, owns = (
            sequence(way.get("EndStencil"), "EndStencil"),
            sequence(way.get("OwnsEnds"), "OwnsEnds"),
        )
        if len(stencil) != 4 or len(owns) != 2:
            raise ValueError("four original stencil points and two OwnsEnds required")
        for raw_point in stencil:
            point = record(raw_point)
            lon, lat = point.get("LongitudeDeg"), point.get("LatitudeDeg")
            if (
                type(lon) not in (int, float)
                or type(lat) not in (int, float)
                or not math.isfinite(lon)
                or not math.isfinite(lat)
                or abs(lon) > 180
                or abs(lat) > 90
            ):
                raise ValueError("actual finite geographic stencil required")
            append("dd", lon, lat)
        append("BB", *(boolean(value) for value in owns))
    return hasher.digest()
