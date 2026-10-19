"""Independently check native capture triangles, attributes and ownership."""

import argparse
import gzip
import json
import math
import struct
from pathlib import Path

VERTEX = struct.Struct("<5fI6f")
POSITION = struct.Struct("<3f")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def expanded_stream(chunk, name):
    data = chunk[name]
    positions, uv, normals = data["positions"], data["uv"], data["normal_words"]
    require(len(positions) == len(uv) == len(normals), "incomplete vertex attributes")
    colours = chunk["wall_colours"] if name == "walls" else []
    metres = chunk["wall_material_metres"] if name == "walls" else []
    require(not colours or len(colours) == 4 * len(positions), "invalid colour extent")
    require(not metres or len(metres) == 2 * len(positions), "invalid material extent")
    vertices = []
    for index, position in enumerate(positions):
        texture, normal = uv[index], normals[index]
        require(len(position) == 3 and len(texture) == 2, "invalid coordinate extent")
        colour = colours[index * 4 : index * 4 + 4] if colours else [1.0] * 4
        material = metres[index * 2 : index * 2 + 2] if metres else [0.0] * 2
        require(
            all(math.isfinite(v) for v in position + texture + colour + material),
            "nonfinite vertex",
        )
        require(type(normal) is int and 0 <= normal < 2**32, "invalid normal word")
        vertices.append(VERTEX.pack(*position, *texture, normal, *colour, *material))
    output = bytearray()
    for triangle in data["triangles"]:
        require(len(triangle) == 3, "incomplete triangle")
        for index in triangle:
            require(
                type(index) is int and 0 <= index < len(vertices),
                "invalid triangle index",
            )
            output.extend(vertices[index])
    return bytes(output)


def expanded_chunks(chunks, name):
    return b"".join(expanded_stream(chunk, name) for chunk in chunks)


def check_capture(call):
    capture = call["capture"]
    members, sources = call["members"], capture["sources"]
    require(bool(sources), "empty native capture")
    require(len(members) == len(sources), "source membership count changed")
    require(
        capture["cube_face_pixels"] > 0 and capture["focal_px"] > 0,
        "invalid projection",
    )
    require(capture["error_px"] > 0, "invalid pixel error")
    require(
        capture["fine_reference_detail"] == 0
        and capture["fine_reference_recessed_openings"] is True,
        "complete native Fine reference required",
    )
    stored = 0
    for slot, source in enumerate(sources):
        require(
            type(source["slot"]) is int and source["slot"] == slot,
            "source slots reordered",
        )
        reference, emitted = source["reference"], source["captured"]
        require(
            "native_fine_reference" in source, "complete native Fine reference missing"
        )
        native = source["native_reference"]
        mode, visible = source["emission"], source["visible_faces"]
        require(type(mode) is int and mode in (0, 1, 2), "invalid emission mode")
        require(
            all(type(v) is int and v in (0, 1) for v in visible),
            "invalid visibility mask",
        )
        for name in ("walls", "roofs"):
            require(
                expanded_chunks(reference, name) == expanded_stream(native, name),
                "reference face ownership differs",
            )
        if mode == 0:
            require(reference == emitted, "whole-source fallback changed")
            require(
                all(c["part"] is None and c["face_index"] is None for c in emitted),
                "invented fine face",
            )
        else:
            require(mode in (1, 2), "unknown emission mode")
            require(
                [c["face_index"] for c in reference] == list(range(len(visible))),
                "incomplete source faces",
            )
            if mode == 1:
                require(
                    emitted == [c for c in reference if visible[c["face_index"]]],
                    "selected faces differ",
                )
            else:
                require(
                    len(emitted) == len(source["patches"]),
                    "patch ownership count changed",
                )
                for patch_index, chunk in enumerate(emitted):
                    patch = source["patches"][patch_index]
                    require(
                        type(chunk["face_index"]) is int
                        and 0 <= chunk["face_index"] < len(reference),
                        "invalid native patch face index",
                    )
                    face = reference[chunk["face_index"]]
                    require(
                        (chunk["part"], chunk["side"])
                        == (patch["part"], patch["side"]),
                        "patch source changed",
                    )
                    require(
                        (face["part"], face["side"]) == (chunk["part"], chunk["side"]),
                        "patch face changed",
                    )
                    parts = [
                        chunk[n] for n in ("walls", "roofs") if chunk[n]["triangles"]
                    ]
                    require(
                        len(parts) == 1 and len(parts[0]["triangles"]) == 2,
                        "patch is not two triangles",
                    )
                    corners = patch["corners"]
                    require(
                        len(corners) == 4 and len(parts[0]["positions"]) == 4,
                        "incomplete patch corners",
                    )
                    require(
                        all(
                            POSITION.pack(*corners[index])
                            == POSITION.pack(*parts[0]["positions"][index])
                            for index in range(len(corners))
                        ),
                        "patch corners differ from native Float32 positions",
                    )
        for chunk in [*reference, *emitted, native, source["native_fine_reference"]]:
            for name in ("walls", "roofs"):
                expanded_stream(chunk, name)
                stored += len(chunk[name]["triangles"])
    for name in ("walls", "roofs"):
        joined = b"".join(
            expanded_chunks(source["captured"], name) for source in sources
        )
        require(
            joined == expanded_stream(capture["native_capture"], name),
            "actual capture differs from owned triangles",
        )
        stored += len(capture["native_capture"][name]["triangles"])
    require(
        stored == capture["stored_triangles"], "serialized triangle accounting differs"
    )
    return {"sources": len(sources), "stored_triangles": stored}


def check(document):
    require(
        all(type(row["index"]) is int for row in document["sources"]),
        "invalid original source index",
    )
    source_ids = {row["index"] for row in document["sources"]}
    require(
        len(source_ids) == len(document["sources"]), "duplicate original source index"
    )
    results = []
    for result in document["results"]:
        require(
            result["accepted"] and result["capture_complete"],
            "native capture export incomplete",
        )
        for call in result["projection_calls"]:
            require(call["accepted"] and "capture" in call, "missing native capture")
            require(
                all(
                    m["source"] is None or m["source"] in source_ids
                    for m in call["members"]
                ),
                "unbound original source",
            )
            results.append(check_capture(call))
    require(bool(results), "no actual native projection")
    return results


def load(path):
    opener = gzip.open if path.suffix == ".gz" else open
    with opener(path, "rt", encoding="utf-8") as file:
        return json.load(file)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(check(load(args.input)), separators=(",", ":")))
