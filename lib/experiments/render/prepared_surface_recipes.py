"""Bounded BSC3 reader for existing prepared geometry and material-coordinate recipes."""

import hashlib
import json
import math
import struct
import tarfile

import numpy as np

SCALAR_FIELDS = {
    code: struct.Struct("<" + code) for code in ("B", "I", "Q", "d", "f", "i")
}


class Reader:
    def __init__(self, data, *, number_limit=1048576):
        if not 0 < len(data) <= 64 * 1024**2:
            raise ValueError("bounded BSC3 block required")
        if type(number_limit) is not int or not 1 <= number_limit <= 16777216:
            raise ValueError("bounded prepared scalar budget required")
        self.data, self.at, self.items, self.numbers = data, 0, 131072, 0
        self.number_limit = number_limit

    def number(self, code):
        field = SCALAR_FIELDS.get(code)
        if field is None:
            field = struct.Struct("<" + code)
        size = field.size
        if self.at + size > len(self.data) or self.numbers >= self.number_limit:
            raise ValueError("truncated or over-budget prepared recipe")
        value = field.unpack_from(self.data, self.at)[0]
        self.at += size
        self.numbers += 1
        if isinstance(value, float) and not math.isfinite(value):
            raise ValueError("nonfinite prepared scalar")
        return value

    def skip(self, size):
        if size < 0 or size > len(self.data) - self.at:
            raise ValueError("truncated prepared data")
        self.at += size

    def text(self):
        self.skip(self.number("Q"))

    def fixed_list(self, size):
        count = self.number("Q")
        self.skip(count * size)
        return count

    def flag(self):
        value = self.number("B")
        if value > 1:
            raise ValueError("invalid prepared presence flag")
        return bool(value)

    def count(self, most, minimum_bytes=1):
        count = self.number("Q")
        if (
            count > most
            or count > self.items
            or count * minimum_bytes > len(self.data) - self.at
        ):
            raise ValueError("invalid prepared list count")
        self.items -= count
        return count

    def vector(self, n=3):
        return [self.number("d") for _ in range(n)]

    def ring(self):
        return [self.vector(2) for _ in range(self.count(4096, 16))]


def read_hip(reader):
    state = reader.number("B")
    if state == 0:
        return None
    if state == 2:
        error = reader.number("i")
        if not 0 <= error <= 4:
            raise ValueError("invalid native hip error")
        return {"error": error}
    if state != 1:
        raise ValueError("invalid hip state")
    result = {
        "origin": reader.vector(2),
        "maximum_distance_m": reader.number("d"),
        "precision_m": reader.number("d"),
    }
    facets = []
    corners = 0
    for _ in range(reader.count(256, 48)):
        facet = {
            "gradient": reader.vector(2),
            "intercept_m": reader.number("d"),
            "source_ring": reader.number("Q"),
            "source_edge": reader.number("Q"),
            "ring": reader.ring(),
        }
        corners += len(facet["ring"])
        if corners > 8192:
            raise ValueError("native hip corner budget exceeded")
        facets.append(facet)
    result["facets"] = facets
    if (
        not facets
        or result["maximum_distance_m"] <= 0
        or not 0 < result["precision_m"] <= 1e-6
    ):
        raise ValueError("invalid native hip plan")
    return result


def read_shape(reader):
    shape = {
        "ring": reader.ring(),
        "holes": [reader.ring() for _ in range(reader.count(4096, 8))],
        "tidied_away": reader.number("Q"),
    }
    shape["party_wall_edges"] = [reader.number("B") for _ in range(reader.count(4096))]
    shape["area_m2"] = reader.number("d")
    shape["centre"] = reader.vector(2)
    shape["axis_u"] = reader.vector(2)
    for name in ("half_u_m", "half_v_m", "fill"):
        shape[name] = reader.number("d")
    shape["form"] = reader.number("B")
    shape["roof_kind"] = reader.number("B")
    shape["storeys"] = reader.number("i")
    for name in (
        "floor_m",
        "foot_m",
        "seat_m",
        "sole_m",
        "eaves_m",
        "rise_m",
        "break_fraction_v",
        "break_rise_m",
        "period_m",
        "bay_m",
        "overhang_m",
    ):
        shape[name] = reader.number("d")
    shape["seed"] = reader.number("I")
    for name in ("ident", "wall_variant", "front_edge", "opening_style"):
        shape[name] = reader.number("i")
    shape["round_footprint"] = reader.flag()
    shape["hip"] = read_hip(reader)
    if (
        len(shape["ring"]) < 3
        or len(shape["party_wall_edges"]) != len(shape["ring"])
        or shape["form"] > 6
        or shape["roof_kind"] > 6
        or shape["storeys"] < 1
        or shape["floor_m"] <= 0
        or shape["bay_m"] <= 0
        or not -1 <= shape["front_edge"] < len(shape["ring"])
    ):
        raise ValueError("invalid native prepared shape")
    return shape


def read_surface(reader):
    begin = reader.at
    if not reader.flag():
        return None
    surface = {name: reader.vector() for name in ("origin", "east", "north", "up")}
    surface["bounds"] = [reader.vector(), reader.vector()]
    surface["minimum_height_m"] = reader.number("d")
    surface["wall_colour"] = (
        [reader.number("f") for _ in range(3)] if reader.flag() else None
    )
    surface["shapes"] = [read_shape(reader) for _ in range(reader.count(4096))]
    surface["face_offsets"] = [reader.number("Q") for _ in range(reader.count(4097, 8))]
    expected = np.cumsum(
        [0]
        + [2 + len(s["ring"]) + sum(map(len, s["holes"])) for s in surface["shapes"]]
    ).tolist()
    if (
        not surface["shapes"]
        or surface["face_offsets"] != expected
        or np.any(np.array(surface["bounds"])[0] > surface["bounds"][1])
    ):
        raise ValueError("invalid native surface bounds/face offsets")
    surface["wire_bytes"] = reader.at - begin
    return surface


def decode(data):
    reader = Reader(data)
    if reader.number("I") != 0x33435342:
        raise ValueError("BSC3 material fields required; no legacy defaults")
    surfaces = [read_surface(reader) for _ in range(reader.count(65536))]
    if reader.at != len(data):
        raise ValueError("trailing prepared surface data")
    return surfaces, {
        "wire_bytes": len(data),
        "scalar_reads": reader.numbers,
        "list_elements": 131072 - reader.items,
    }


def bind(surfaces, document):
    sources = document["sources"]
    if len(surfaces) != len(sources):
        raise ValueError("whole-cell source count differs")
    if [x["index"] for x in sources] != sorted(x["index"] for x in sources):
        raise ValueError("original source-cell order required")
    result = {}
    keys = ("origin", "east", "north", "up", "minimum_height_m")
    shape_keys = (
        "ring",
        "holes",
        "party_wall_edges",
        "centre",
        "axis_u",
        "half_u_m",
        "half_v_m",
        "rise_m",
        "roof_kind",
        "break_fraction_v",
        "break_rise_m",
        "period_m",
    )
    for raw, source in zip(surfaces, sources, strict=True):
        given = source["surface"]
        if (raw is None) != (given is None):
            raise ValueError("unsupported surface identity differs")
        if raw is not None:
            if any(raw[k] != given[k] for k in keys) or len(raw["shapes"]) != len(
                given["shapes"]
            ):
                raise ValueError("prepared frame/source association differs")
            for a, b in zip(raw["shapes"], given["shapes"], strict=True):
                if any(a[k] != b[k] for k in shape_keys):
                    raise ValueError("prepared shape association differs")
                hip = b.get("hip")
                if hip and hip["available"]:
                    raw_hip = a["hip"]
                    if (
                        not raw_hip
                        or "error" in raw_hip
                        or any(
                            raw_hip[k] != hip[k]
                            for k in ("origin", "maximum_distance_m", "precision_m")
                        )
                        or len(raw_hip["facets"]) != len(hip["facets"])
                    ):
                        raise ValueError("prepared hip association differs")
                    for x, y in zip(raw_hip["facets"], hip["facets"], strict=True):
                        if any(
                            x[k] != y[k]
                            for k in (
                                "gradient",
                                "intercept_m",
                                "source_ring",
                                "source_edge",
                                "ring",
                            )
                        ):
                            raise ValueError("prepared hip facet differs")
        if source["index"] in result:
            raise ValueError("duplicate source identity")
        result[source["index"]] = raw
    return result


def load_recipes(path, document):
    """Read just the existing cell block; caller also validates the full native view."""
    root = path.parent
    archive_path = root / "original-inputs.tar.gz"
    manifest = json.loads((root / "manifest.json").read_text())[
        "original-inputs.tar.gz"
    ]
    data = archive_path.read_bytes()
    if (
        len(data) != manifest["bytes"]
        or hashlib.sha256(data).hexdigest() != manifest["sha256"]
    ):
        raise ValueError("original archive manifest differs")
    cells = {s["cell"] for s in document["sources"]}
    if len(cells) != 1:
        raise ValueError("one complete supplied cell required")
    with tarfile.open(archive_path) as archive:
        metadata = json.load(archive.extractfile(document["parent"] + "/input.json"))
        if (
            metadata["parent"] != document["parent"]
            or metadata["view_key"] != document["view_key"]
        ):
            raise ValueError("original parent/view identity differs")
        form = next(x for x in metadata["forms"] if x["cell"] == next(iter(cells)))
        member = archive.getmember(document["parent"] + "/" + form["file"])
        if not member.isfile() or member.size > 64 * 1024**2:
            raise ValueError("bounded native form block required")
        block = archive.extractfile(member).read()
    record = next(x for x in metadata["native_packages"] if x["file"] == form["file"])
    if (
        record["native_bytes"] != len(block)
        or record["sha256"] != hashlib.sha256(block).hexdigest()
    ):
        raise ValueError("native form package digest differs")
    surfaces, cost = decode(block)
    bound = bind(surfaces, document)
    return bound, cost | {
        "block": form["file"],
        "sha256": hashlib.sha256(block).hexdigest(),
        "original_package_crc": record["package_crc"],
        "package_crc_checked": False,
    }
