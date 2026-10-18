"""Opt in to one externally pinned terrain product; unrelated inputs stay strict."""

import contextlib
import hashlib
import json
from pathlib import Path
from unittest.mock import patch

import stair_local_assets
from camera_height_inputs import digest
from outshine_ref.contracts import LocalFrame
from outshine_ref.geometry.surface_patches._archive import MAX_META, unique_object
from outshine_ref.geometry.surface_patches.prepared import (
    FORMAT,
    MAX_BYTES,
    restore_surface,
)

REFERENCE = "outshine-prepared-terrain-reference/1"


def optional_terrain(args, records):
    path = getattr(args, "prepared_reference", None)
    pin = getattr(args, "reference_sha256", None)
    if path is None and pin is None:
        return contextlib.nullcontext()
    if path is None or pin is None:
        raise ValueError("prepared reference and independent external pin required")
    return admitted_terrain(path, pin, args.prepared_variant, records)


def reference(path, expected_sha256):
    if not isinstance(expected_sha256, str) or len(expected_sha256) != 64:
        raise ValueError("independent external reference digest required")
    if path.stat().st_size > MAX_META:
        raise ValueError("bounded external prepared reference required")
    payload = path.read_bytes()
    if hashlib.sha256(payload).hexdigest() != expected_sha256:
        raise ValueError("external prepared reference pin differs")
    result = json.loads(payload, object_pairs_hook=unique_object)
    if result["reference_format"] != REFERENCE or result["codec_format"] != FORMAT:
        raise ValueError("known independently checked prepared reference required")
    if set(result["variants"]) != {"stored", "compressed"}:
        raise ValueError("complete independently compared surface variants required")
    return result


@contextlib.contextmanager
def admitted_terrain(path, expected_sha256, variant, records):
    if variant not in ("stored", "compressed"):
        raise ValueError("explicit admitted representation required")
    original = stair_local_assets.load_prepared
    proof, cached = None, None

    def load(terrain):
        nonlocal proof, cached
        if proof is None:
            proof = reference(Path(path), expected_sha256)
        if Path(terrain).resolve() != Path(proof["terrain"]).resolve():
            return original(terrain)
        resident = Path(proof["resident"])
        if (
            digest(resident / "resident.json") != proof["resident_sha256"]
            or digest(Path(terrain) / "patches.json") != proof["terrain_sha256"]
        ):
            raise ValueError(
                "externally pinned original resident/terrain binding changed"
            )
        if cached is not None:
            records.append({"restored": False, "reused": True})
            return cached
        row = proof["variants"][variant]
        products = Path(proof["products"])
        payload = (products / row["file"]).resolve()
        if (
            not payload.is_relative_to(products.resolve())
            or payload.stat().st_size != row["bytes"]
            or payload.stat().st_size > MAX_BYTES
        ):
            raise ValueError("bounded independently pinned numeric payload required")
        view = restore_surface(
            payload.read_bytes(),
            expected_sha256=row["sha256"],
            frame=LocalFrame(proof["frame"]),
        )
        manifest = json.loads((Path(terrain) / "patches.json").read_text())
        if manifest["frame"] != proof["frame"]:
            raise ValueError("original prepared terrain frame changed")
        cached = view, manifest
        records.append(
            {
                "restored": True,
                "reused": False,
                "bytes": row["bytes"],
                "patches": len(view.patches),
            }
        )
        return cached

    with patch.object(stair_local_assets, "load_prepared", load):
        yield
