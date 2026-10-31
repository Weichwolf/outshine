"""Bind the finite native Lens/control record before scoring clip decisions.

Only native matrix/pre-view words feed the selector. Image/counter observations
remain separate scores. This record is a test input, not runtime admission.
"""

import hashlib
import json

import numpy as np
from native_source_triangle_materials import DATA


def bind(record, base_words, manifest):
    if (
        record.get("version") != 1
        or record.get("source_fixture_manifest_sha256")
        != hashlib.sha256(manifest).hexdigest()
        or base_words.dtype != np.dtype("<u4")
        or base_words.shape != (56,)
        or not isinstance(record.get("controls"), list)
        or len(record["controls"]) != 13
    ):
        raise ValueError("Bound native clip fixture and original view words required")
    result = {}
    for row in record["controls"]:
        angle = row["angle"]
        if type(angle) is not int or not 0 <= angle < 360 or angle in result:
            raise ValueError("Unique bounded native control angles required")
        words = base_words.copy()
        for key, span, length in (
            ("projection_words_u32", slice(0, 16), 16),
            ("pre_view_words_u32", slice(48, 51), 3),
        ):
            values = row.get(key)
            if (
                not isinstance(values, list)
                or len(values) != length
                or any(type(v) is not int or not 0 <= v <= 0xFFFFFFFF for v in values)
            ):
                raise ValueError("Exact native UInt32 operand words required")
            words[span] = values
        result[angle] = words
    return result


def read(base_words, directory=DATA):
    path = directory / "native-clip-controls.json"
    if path.stat().st_size > 65536:
        raise ValueError("Bounded native control record required")
    record = json.loads(path.read_text())
    return bind(record, base_words, (directory / "manifest.json").read_bytes()), record
