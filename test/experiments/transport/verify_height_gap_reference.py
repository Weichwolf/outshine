"""Read unchanged portable whole-face pins without SDK or recipe regeneration."""

import hashlib
import json
from pathlib import Path

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import vertical_clearance

ROOT = Path(__file__).resolve().parents[3]
REFERENCE = Path(__file__).with_name("reference") / "height-gap"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    metadata = json.loads((REFERENCE / "metadata.json").read_text())
    assert digest(REFERENCE / "pairs.npz") == metadata["pairs_sha256"]
    assert digest(REFERENCE / "pins.json") == metadata["pins_sha256"]
    for fixture in metadata["fixtures"]:
        assert digest(ROOT / fixture["path"]) == fixture["sha256"]
    expected = json.loads((REFERENCE / "pins.json").read_text())["results"]
    assert len(expected) == len(metadata["cases"]) == metadata["mesh_pairs"] == 25
    with np.load(REFERENCE / "pairs.npz", allow_pickle=False) as data:
        used = set()
        for case, pin in zip(metadata["cases"], expected, strict=True):
            assert case["name"] == pin["name"]
            surfaces = []
            for role in ("lower", "upper"):
                arrays = []
                for part in ("vertices", "faces"):
                    spec = case[role]["arrays"][part]
                    array = data[spec["key"]]
                    used.add(spec["key"])
                    assert list(array.shape) == spec["shape"]
                    assert array.dtype.str == spec["dtype"]
                    assert hashlib.sha256(array.tobytes()).hexdigest() == spec["sha256"]
                    arrays.append(array)
                assert all(
                    s in metadata["source_catalog"] for s in case[role]["source_ids"]
                )
                surfaces.append(Mesh(*arrays))
            result = vertical_clearance(*surfaces)
            assert result["valid"], (case["name"], result)
            assert result["minimum_clearance_m"] == pin["minimum_clearance_m"]
            assert result["witness_xy"] == pin["witness_xy"]
        assert used == set(data.files)
    print(
        json.dumps(
            {
                "pairs": len(expected),
                "sha256": metadata["pairs_sha256"],
                "exact_minima_witnesses": True,
            }
        )
    )


if __name__ == "__main__":
    main()
