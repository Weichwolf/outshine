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
    if schema == "outshine/native-mvt-source-facts/3":
        return 3
    if schema == "outshine/native-mvt-source-facts/4":
        return 4
    raise ValueError("known native source fact schema required")


def native_source_fact_digest(
    revision,
    partition_digest,
    payload,
    *,
    member_limit=65536,
    row_limit=16384,
    point_limit=65536,
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
    if type(version) is not int or version not in (1, 2, 3, 4):
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
    missing = (
        sequence(payload.get("MissingWays"), "MissingWays") if version >= 3 else ()
    )
    if version >= 3:
        if len(missing) > row_limit:
            raise ValueError("missing original way budget")
        hasher.update(digest_bytes(payload.get("OmissionsDigest")))
        append("Q", len(missing))
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
        if version >= 2:
            append("B", boolean(member.get("SourceTerminal")))

    def append_way(raw):
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

    for raw in ways:
        append_way(raw)
    point_count = 0
    for raw in missing:
        row = record(raw)
        append("I", integer(row.get("Omission"), 32))
        append_way(row.get("Way"))
        layer = row.get("Layer")
        if type(layer) is not int or not -(2**31) <= layer < 2**31:
            raise ValueError("native signed layer required")
        points = sequence(row.get("Points"), "Points")
        point_count += len(points)
        if (
            point_count > point_limit
            or len(points) != row["Way"]["PointCount"]
            or len(points) < 2
        ):
            raise ValueError("bounded complete original way points required")
        append(
            "iBBQ",
            layer,
            boolean(row.get("Bridge")),
            boolean(row.get("Tunnel")),
            len(points),
        )
        for raw_point in points:
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
                raise ValueError("actual finite original geographic points required")
            append("dd", lon, lat)
        if version == 4:
            append_original_points(append, row, len(groups), len(points))
    return hasher.digest()


def append_original_points(append, row, group_count, point_count):
    """Bind native point states, not source absence or influence-free closure."""
    state = integer(row.get("OriginalWayState"), 8)
    if state > 4:
        raise ValueError("known original way state required")
    points = sequence(row.get("OriginalPoints"), "OriginalPoints")
    if len(points) != point_count:
        raise ValueError(
            "one original point state per complete polyline point required"
        )
    append("BQ", state, len(points))
    for raw in points:
        point = record(raw)
        point_state = integer(point.get("State"), 8)
        group = integer(point.get("Group"), 32)
        if point_state > 2 or (state != 1 and point_state != 0):
            raise ValueError("point assignment requires an exact original way")
        if (point_state == 2 and group >= group_count) or (
            point_state != 2 and group != 2**32 - 1
        ):
            raise ValueError(
                "published original point group or exact unknown sentinel required"
            )
        append("BI", point_state, group)


def digest_bytes(raw):
    values = sequence(raw, "native digest")
    if len(values) != 32:
        raise ValueError("actual 32-byte digest required")
    return bytes(integer(value, 8) for value in values)


def native_omissions_digest(omissions, *, row_limit=16384):
    rows = sequence(omissions, "Omissions")
    if len(rows) > row_limit:
        raise ValueError("original omission budget")
    h = hashlib.sha256(b"outshine/mvt-source-omissions/1")
    h.update(struct.pack("<Q", len(rows)))
    previous = None
    for raw in rows:
        row = record(raw)
        source = integer(row.get("Source"), 32)
        feature = digest_bytes(row.get("Feature"))
        fragment = row.get("Fragment")
        fragment = digest_bytes(fragment) if fragment is not None else None
        reason = integer(row.get("Reason"), 8)
        key = (source, feature, fragment is not None, fragment or b"", reason)
        if previous is not None and key <= previous:
            raise ValueError("original omission order must be canonical and unique")
        previous = key
        h.update(struct.pack("<I", source))
        h.update(feature)
        h.update(struct.pack("<B", fragment is not None))
        if fragment is not None:
            h.update(fragment)
        h.update(struct.pack("<B", reason))
    return h.digest()


def validate_missing_source_ways(native, payload, *, row_limit=16384):
    """Check paired correspondence after native_source_fact_digest validates fields."""
    version = version_from_schema(payload["schema"])
    if version < 3:
        return
    omissions = sequence(native.get("Omissions"), "Omissions")
    if native_omissions_digest(omissions, row_limit=row_limit) != digest_bytes(
        payload.get("OmissionsDigest")
    ):
        raise ValueError("paired original omission digest mismatch")
    represented = {row["StreetWay"] for row in payload["Ways"]}
    previous, used = -1, set()
    missing = sequence(payload.get("MissingWays"), "MissingWays")
    if len(missing) > row_limit:
        raise ValueError("missing original way budget")
    for raw in missing:
        row = record(raw)
        if version == 3 and ("OriginalWayState" in row or "OriginalPoints" in row):
            raise ValueError("original point states require a v4 digest binding")
        index = integer(row.get("Omission"), 32)
        way = record(row.get("Way"))
        street = integer(way.get("StreetWay"), 32)
        if (
            index >= len(omissions)
            or street <= previous
            or street in represented
            or street >= payload["SourceWayCount"]
        ):
            raise ValueError("unique canonical original way and omission required")
        omission = omissions[index]
        if omission["Reason"] != 5 or omission.get("Fragment") is None:
            raise ValueError(
                "original geometry requires an exact missing topology identity"
            )
        points = sequence(row.get("Points"), "Points")
        if len(points) < 2 or len(points) != way["PointCount"]:
            raise ValueError("complete original polyline required")
        stencil = [points[0], points[1], points[-2], points[-1]]

        def words(point):
            return struct.pack("<dd", point["LongitudeDeg"], point["LatitudeDeg"])

        if [words(p) for p in stencil] != [words(p) for p in way["EndStencil"]]:
            raise ValueError("original polyline and end stencil disagree")
        used.add(index)
        previous = street
    expected = {
        index
        for index, row in enumerate(omissions)
        if row["Reason"] == 5 and row.get("Fragment") is not None
    }
    if used != expected:
        raise ValueError("every exactly identified missing way requires original facts")
