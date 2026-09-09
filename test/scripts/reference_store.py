"""Content-addressed reference images; test runs never create or update pins."""
import hashlib
import json
import math
import os
from pathlib import Path
import tempfile

from PIL import Image


def directory():
    return Path(os.environ.get("OUTSHINE_REFERENCE_CACHE", str(Path(tempfile.gettempdir()) / "outshine-reference-images")))


def resolve(record, cache=None):
    digest = record["sha256"]
    if len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
        raise ValueError("invalid reference SHA-256")
    path = (Path(cache) if cache is not None else directory()) / digest
    if not path.is_file():
        raise ValueError(f"missing pinned reference {digest}; populate the reference cache explicitly")
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != digest:
        raise ValueError(f"corrupt pinned reference {digest}")
    with Image.open(path) as image:
        if image.size != (record["widthPx"], record["heightPx"]):
            raise ValueError(f"reference dimensions disagree with pin {digest}")
    return path


def pin(image_path, frame, seconds, cache=None):
    data = Path(image_path).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    target = Path(cache) if cache is not None else directory()
    target.mkdir(parents=True, exist_ok=True)
    with Image.open(image_path) as image:
        width, height = image.size
    record = dict(frame=frame, seconds=seconds, sha256=digest, widthPx=width, heightPx=height)
    fd, temporary = tempfile.mkstemp(dir=target, prefix=".reference-")
    try:
        with os.fdopen(fd, "wb") as out:
            out.write(data)
        os.replace(temporary, target / digest)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
    resolve(record, target)
    return record


def verify_declared_references(tree):
    count, unpinned = 0, []
    for path in sorted(Path(tree).glob("*/*/manifest.json")):
        manifest = json.loads(path.read_text())
        if not manifest.get("renders"):
            continue
        records = manifest.get("referenceImages", [])
        if not records:
            unpinned.append(path.parent.name)
            continue
        animation = manifest.get("scene", {}).get("animation", {})
        frames = animation.get("frames", {}).get("value", 1)
        fps = animation.get("fps", {}).get("value", 1)
        if [record["frame"] for record in records] != list(range(frames)):
            raise ValueError(f"incomplete or duplicate frame pins: {path}")
        recipe = manifest["renders"]["default"]
        for record in records:
            if not math.isfinite(record["seconds"]) or record["seconds"] != record["frame"] / fps:
                raise ValueError(f"frame time disagrees with setup: {path}")
            if (record["widthPx"], record["heightPx"]) != (recipe["resolutionX"], recipe["resolutionY"]):
                raise ValueError(f"reference dimensions disagree with setup: {path}")
            resolve(record)
            count += 1
    if count == 0:
        raise ValueError("no declared image references were checked")
    print(f"{count} pinned images verified; no pins declared for: {', '.join(unpinned) or 'none'}")


if __name__ == "__main__":
    verify_declared_references(Path(__file__).resolve().parents[2] / "test/khronos")
