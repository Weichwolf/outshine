"""Check the captured inputs without generating expected solver results."""

import gzip
import hashlib
import json
import zipfile
from pathlib import Path


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify(root):
    manifest = json.loads((root / "input.json").read_text())
    require(manifest["schema"] == "outshine/feldkirch-shore-contact/1", "schema")
    for name, expected in manifest["files"].items():
        require(Path(name).name == name, "local input name")
        data = (root / name).read_bytes()
        require(len(data) == expected["bytes"], name + " size")
        require(hashlib.sha256(data).hexdigest() == expected["sha256"], name + " hash")
    with gzip.open(root / "contact.json.gz", "rb") as source:
        raw = source.read(32 * 1024**2 + 1)
    require(len(raw) <= 32 * 1024**2, "bounded contact")
    contact = json.loads(raw)
    require(contact["schema"] == 1, "contact schema")
    stamps = contact["stamps"]
    ordinals = [s["ordinal"] for s in stamps]
    require(ordinals == sorted(set(ordinals)), "original unique stamp order")
    require(
        all(0 <= i < contact["full_stamp_count"] for i in ordinals), "ordinal range"
    )
    by_id = {s["ordinal"]: s for s in stamps}
    require(by_id[46764]["kind"] == 2, "observed basin identity")
    require(by_id[60942]["kind"] == 1, "observed corridor identity")
    require(all(by_id[i]["profile"] is None for i in (46764, 60942)), "observed path")
    water = contact["water"]
    vertices, faces = water["vertices"], water["faces"]
    require(len(faces) == contact["basins"], "one basin stamp per original water face")
    require(all(len(v) == 3 for v in vertices), "water positions")
    require(
        all(len(f) == 4 and all(0 <= i < len(vertices) for i in f[:3]) for f in faces),
        "water face vertex references",
    )
    offsets = water["face_source_offsets"]
    require(
        len(offsets) == len(faces) + 1
        and offsets[0] == 0
        and offsets[-1] == len(water["face_sources"])
        and offsets == sorted(offsets),
        "water source incidence",
    )
    with zipfile.ZipFile(root / "deformed-pages.npz") as archive:
        require(
            set(archive.namelist()) == {p["array"] + ".npy" for p in manifest["pages"]},
            "declared native pages",
        )
        require(archive.testzip() is None, "page archive CRC")
    return {
        "stamps": len(stamps),
        "water_faces": len(faces),
        "pages": len(manifest["pages"]),
        "state": "captured_inputs_verified",
    }


if __name__ == "__main__":
    print(json.dumps(verify(Path(__file__).resolve().parent)))
