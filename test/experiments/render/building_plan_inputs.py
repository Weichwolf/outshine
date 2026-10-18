"""Read enriched native source plans and selection summaries without emitting meshes."""

import struct

import numpy as np
from cached_building_streams import Reader, array


def header(reader):
    anchor = tuple(reader.number("d") for _ in range(3))
    reader.skip(8 + 4 + 1 + 8 + 4 + 32 + 8)
    if reader.flag():
        reader.text()
        reader.text()
        for _ in range(reader.number("Q")):
            reader.text()
        if reader.flag():
            reader.skip(12)
    for _ in range(reader.number("Q")):
        reader.skip(4 + 1 + 12)
        if reader.flag():
            reader.skip(8)
        reader.text()
        reader.text()
    reader.skip(8 + 1 + 4 + 1)
    reader.fixed_list(12)
    return anchor


def prepared(reader):
    local, count, source, first_hole, holes, _ = (reader.number("I") for _ in range(6))
    cell = reader.number("I")
    reader.skip(32)
    height, minimum = reader.number("d"), reader.number("d")
    pitched = reader.number("i")
    colour = tuple(reader.number("f") for _ in range(3)) if reader.flag() else None
    origin = reader.number("B") if reader.flag() else None
    identity, kind = reader.number("Q"), reader.number("B")
    facade = reader.number("i") if reader.flag() else None
    standing = struct.unpack_from("<4I5fBB6d", reader.data, reader.at)
    reader.skip(86)
    geographic_bounds = tuple(reader.number("d") for _ in range(4))
    first_corner = reader.number("Q")
    base, seat, area, across = (reader.number("d") for _ in range(4))
    return {
        "source_id": (kind, identity),
        "source_first": source,
        "cell": cell,
        "local_first": local,
        "point_count": count,
        "first_hole": first_hole,
        "hole_count": holes,
        "height_m": height,
        "minimum_height_m": minimum,
        "pitched": pitched,
        "colour": colour,
        "height_origin": origin,
        "facade": facade,
        "standing": standing,
        "geographic_bounds": geographic_bounds,
        "corner_first": first_corner,
        "base_asl_m": base,
        "seat_asl_m": seat,
        "area_m2": area,
        "across_m": across,
    }


def summary(reader):
    if not reader.flag():
        return {
            "supported": False,
            "envelope": None,
            "shell_error_m": None,
            "faces": 0,
            "projectable": False,
        }
    frame = np.array([reader.number("d") for _ in range(12)]).reshape(4, 3)
    bounds = np.array([reader.number("d") for _ in range(6)]).reshape(2, 3)
    minimum = reader.number("d")
    colour = tuple(reader.number("f") for _ in range(3)) if reader.flag() else None
    envelope = (
        np.array([reader.number("d") for _ in range(6)]).reshape(2, 3)
        if reader.flag()
        else None
    )
    error = reader.number("d") if reader.flag() else None
    return {
        "supported": True,
        "frame": frame,
        "bounds": bounds,
        "minimum_height_m": minimum,
        "colour": colour,
        "envelope": envelope,
        "shell_error_m": error,
        "faces": reader.number("I"),
        "projectable": bool(reader.flag()),
    }


def read_plans(data):
    if len(data) > 64 * 1024**2:
        raise ValueError("bounded native source index required")
    reader = Reader(data)
    if (reader.number("I"), reader.number("I")) != (0x31425350, 8):
        raise ValueError("explicitly supported prepared index schema 8 required")
    anchor = header(reader)
    points, _ = array(reader, 8)
    holes, _ = array(reader, 9)
    corners, _ = array(reader, 8)
    count = reader.number("Q")
    if count > 262144:
        raise ValueError("bounded source record count required")
    records = [prepared(reader) for _ in range(count)]
    if reader.number("Q") != len(records):
        raise ValueError("selection summaries must cover all enriched source records")
    summaries = [summary(reader) for _ in records]
    if reader.at != len(data):
        raise ValueError("unexpected prepared index schema or trailing bytes")
    result = {
        "anchor_ecef": anchor,
        "points_latlon": np.frombuffer(points, dtype="<f8").reshape(-1, 2),
        "holes": np.frombuffer(
            holes, dtype=[("first", "<u4"), ("count", "<u4"), ("exterior", "u1")]
        ),
        "corner_asl_m": np.frombuffer(corners, dtype="<f8"),
        "records": records,
        "summaries": summaries,
    }
    validate_plans(result)
    return result


def validate_plans(plans):
    points, holes, corners = (
        plans["points_latlon"],
        plans["holes"],
        plans["corner_asl_m"],
    )
    if not np.isfinite(points).all() or not np.isfinite(corners).all():
        raise ValueError("finite original coordinates/heights required")
    for hole in holes:
        if int(hole["first"]) + int(hole["count"]) > len(points):
            raise ValueError("inner ring escapes its original coordinate array")
    for record, summary in zip(plans["records"], plans["summaries"], strict=True):
        if (
            not 1 <= record["cell"] <= 64
            or record["local_first"] + record["point_count"] > len(points)
            or record["first_hole"] + record["hole_count"] > len(holes)
            or record["corner_first"] + record["point_count"] > len(corners)
        ):
            raise ValueError("source/cell/ring/corner range escapes the original plan")
        bounds, error = summary["envelope"], summary["shell_error_m"]
        if bounds is not None and (
            not np.isfinite(bounds).all() or (bounds[0] > bounds[1]).any()
        ):
            raise ValueError("invalid declared source envelope")
        if error is not None and (not np.isfinite(error) or error < 0):
            raise ValueError("invalid declared shell error")
