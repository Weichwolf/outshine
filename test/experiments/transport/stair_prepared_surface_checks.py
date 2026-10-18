"""Research-only external reference and exact terrain-binding scope controls."""

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import stair_local_assets
import test_surface_patches
from outshine_ref.geometry.surface_patches import SurfacePatches
from outshine_ref.geometry.surface_patches.prepared import FORMAT, pack_surface
from stair_prepared_surface_scope import REFERENCE, admitted_terrain
from stair_resident_snapshot_verify import equal_patches
from test_prepared_surface import sha


class ScopedPreparedSurface(unittest.TestCase):
    def setUp(self):
        root = test_surface_patches.patch("root1")
        self.view, _ = SurfacePatches(root).replace(
            test_surface_patches.refined("next2"), {"root1": [0, 1]}
        )
        self.frame = root.frame
        self.payload = pack_surface(self.view)
        self.pin = sha(self.payload)

    def scoped_reference(self):
        temporary = tempfile.TemporaryDirectory(
            prefix="outshine-transport-scope-", dir="/tmp"
        )
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        terrain = root / "terrain"
        terrain.mkdir()
        (root / "resident.json").write_text('{"source_ids":["original"]}')
        manifest = json.dumps({"frame": self.frame.identifier}).encode()
        (terrain / "patches.json").write_bytes(manifest)
        (root / "surface.npz").write_bytes(self.payload)
        description = {
            "reference_format": REFERENCE,
            "codec_format": FORMAT,
            "terrain": str(terrain),
            "terrain_sha256": sha(manifest),
            "resident": str(root),
            "resident_sha256": sha((root / "resident.json").read_bytes()),
            "products": str(root),
            "frame": self.frame.identifier,
            "variants": {
                name: {
                    "file": "surface.npz",
                    "bytes": len(self.payload),
                    "sha256": self.pin,
                }
                for name in ("stored", "compressed")
            },
        }
        proof = root / "reference.json"
        proof.write_text(json.dumps(description))
        return proof, sha(proof.read_bytes()), terrain

    def test_scoped_hit_reuses_only_matching_pinned_terrain_and_restores_loader(self):
        proof, pin, terrain = self.scoped_reference()
        records = []
        with patch.object(
            stair_local_assets, "load_prepared", return_value="strict"
        ) as old:
            with admitted_terrain(proof, pin, "stored", records):
                first = stair_local_assets.load_prepared(terrain)
                second = stair_local_assets.load_prepared(terrain)
                self.assertIs(first[0], second[0])
                equal_patches(first[0], self.view)
                self.assertEqual(
                    stair_local_assets.load_prepared(terrain / "other"), "strict"
                )
                old.assert_called_once_with(terrain / "other")
            self.assertIs(stair_local_assets.load_prepared, old)
        self.assertEqual([row["restored"] for row in records], [True, False])

    def test_scoped_hit_refuses_changed_reference_original_binding_and_payload(self):
        for changed in (
            "reference.json",
            "resident.json",
            "terrain/patches.json",
            "surface.npz",
        ):
            proof, pin, terrain = self.scoped_reference()
            path = proof.parent / changed
            path.write_bytes(path.read_bytes() + b" ")
            with (
                admitted_terrain(proof, pin, "stored", []),
                self.assertRaises(ValueError),
            ):
                stair_local_assets.load_prepared(terrain)


if __name__ == "__main__":
    unittest.main()
