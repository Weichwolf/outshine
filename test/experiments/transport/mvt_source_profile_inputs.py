"""Read only native GroundProfile tags, bound by original feature wire SHA256."""

import hashlib
import struct

from osm_provider_inventory import fields, packed


def native_value(body):
    """MvtLayer::ReadValue: one scalar type, last same-type value wins."""
    found, value = None, None
    for number, wire, raw in fields(body):
        if (number, wire) not in (
            (1, 2),
            (2, 5),
            (3, 1),
            (4, 0),
            (5, 0),
            (6, 0),
            (7, 0),
        ):
            continue
        if found not in (None, number):
            raise ValueError("native MVT scalar contains different value types")
        found = number
        if number == 1:
            value = bytes(raw).decode("utf8")
        elif number in (2, 3):
            value = struct.unpack("<f" if number == 2 else "<d", raw)[0]
        elif number == 4:
            value = float(raw if raw < 2**63 else raw - 2**64)
        elif number == 5:
            value = float(raw)
        elif number == 6:
            value = float((raw >> 1) ^ -(raw & 1))
        else:
            value = float(raw != 0)
    if found is None:
        raise ValueError("missing native MVT scalar")
    return value


def profile_properties(raw, *, byte_limit=4 * 1024 * 1024, tag_word_limit=1048576):
    """Index OpenMapTiles transportation features; no geometry decoding/copying.

    FeatureContent is exactly Sha256Digest(original feature body) in native
    MvtLayer::DecodeFeatures. Duplicate attribute names use the first value,
    matching OsmField::FindTag. indoor/level have no native key aliases.
    """
    if not isinstance(raw, bytes) or len(raw) > byte_limit:
        raise ValueError("source MVT byte budget")
    result = {}
    work = {
        "source_bytes": len(raw),
        "wire_features": 0,
        "tag_words": 0,
        "decoded_geometry_words": 0,
        "additional_geometry_payload_copy_bytes": 0,
    }
    for number, wire, body in fields(memoryview(raw)):
        if (number, wire) != (3, 2):
            continue
        layer = list(fields(body))
        names = [bytes(v) for n, w, v in layer if (n, w) == (1, 2)]
        if names != [b"transportation"]:
            continue
        keys = [bytes(v).decode("utf8") for n, w, v in layer if (n, w) == (3, 2)]
        values = [v for n, w, v in layer if (n, w) == (4, 2)]
        for n, w, feature in layer:
            if (n, w) != (2, 2):
                continue
            tags = []
            for key, mode, data in fields(feature):
                if key == 2:
                    if mode == 2:
                        for word in packed(data):
                            work["tag_words"] += 1
                            if work["tag_words"] > tag_word_limit:
                                raise ValueError("source MVT tag budget")
                            tags.append(word)
                    elif mode == 0:
                        work["tag_words"] += 1
                        if work["tag_words"] > tag_word_limit:
                            raise ValueError("source MVT tag budget")
                        tags.append(data)
                    else:
                        raise ValueError("invalid MVT tags")
            if len(tags) % 2 or work["tag_words"] > tag_word_limit:
                raise ValueError("source MVT tag budget or odd tags")
            properties = {}
            for i in range(0, len(tags), 2):
                if tags[i] >= len(keys) or tags[i + 1] >= len(values):
                    raise ValueError("invalid MVT tag index")
                key = keys[tags[i]]
                if key in ("indoor", "level") and key not in properties:
                    properties[key] = native_value(values[tags[i + 1]])
            digest = hashlib.sha256(feature).digest()
            if digest in result and result[digest] != properties:
                raise ValueError("conflicting source feature content")
            result[digest] = properties
            work["wire_features"] += 1
    return result, work
