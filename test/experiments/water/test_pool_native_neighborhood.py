"""Original neighbor words, complete archive identity and bounded mismatch refusal."""

import hashlib
import io
import json
import tarfile
import tempfile
import unittest
from pathlib import Path

import numpy as np
from outshine_ref.water.geodetic import local_points
from pool_body_experiment import FIXTURE as BODY_FIXTURE
from pool_body_experiment import original
from pool_native_neighborhood import FIXTURE, load_neighborhood


class PoolNativeNeighborhood(unittest.TestCase):
    def test_original_window_shares_words_and_reproduces_the_old_cell(self):
        result = load_neighborhood()
        self.assertEqual(result.heights.shape, (97, 97))
        self.assertEqual(result.vertices.shape, (9409, 3))
        self.assertEqual(result.faces.shape, (18432, 3))
        self.assertEqual(result.report["shared_original_height_occurrences"], 392)
        data, _, _, _, _ = original(BODY_FIXTURE)
        for key in ("asset_key", "stored_sha256", "native_sha256"):
            self.assertEqual(result.manifest[key], data["source"][key])
        indices = np.array([30 * 97 + 34, 30 * 97 + 35, 31 * 97 + 34, 31 * 97 + 35])
        actual = result.heights.ravel()[indices]
        expected = np.array(data["terrain"]["cellCornersM"], dtype="<f4").ravel()
        self.assertEqual(actual.tobytes(), expected.tobytes())
        lonlat = np.array([[p["lon"], p["lat"]] for p in data["water"]["points"]])
        previous = local_points(result.manifest["anchor_lon_lat"], lonlat, expected)
        self.assertLess(float(abs(result.vertices[indices] - previous).max()), 1e-7)

    def modified(self, directory, change):
        with tarfile.open(FIXTURE, "r:xz") as source:
            files = {m.name: source.extractfile(m).read() for m in source.getmembers()}
        manifest = json.loads(files["input.json"])
        change(files, manifest)
        files["input.json"] = json.dumps(manifest).encode()
        path = Path(directory) / "changed.tar.xz"
        with tarfile.open(path, "w:xz") as target:
            for name, raw in files.items():
                entry = tarfile.TarInfo(name)
                entry.size = len(raw)
                target.addfile(entry, io.BytesIO(raw))
        metadata = json.loads(FIXTURE.with_suffix(".json").read_text())
        metadata["archive_sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
        path.with_suffix(".json").write_text(json.dumps(metadata))
        return path

    def test_changed_missing_and_duplicated_original_inputs_are_refused(self):
        def missing(files, _):
            files.pop("185.f32")

        def changed(files, _):
            raw = bytearray(files["185.f32"])
            raw[0] ^= 1
            files["185.f32"] = bytes(raw)

        def duplicate(_, manifest):
            manifest["rows"][1] = manifest["rows"][0]

        for change in (missing, changed, duplicate):
            with (
                self.subTest(change=change.__name__),
                tempfile.TemporaryDirectory() as directory,
                self.assertRaises(ValueError),
            ):
                load_neighborhood(self.modified(directory, change))

    def test_individually_rehashed_but_inconsistent_shared_words_are_refused(self):
        def mismatch(files, manifest):
            page = next(p for p in manifest["rows"] if p["row"]["page"] == "185")
            field = np.frombuffer(files["185.f32"], dtype="<f4").copy().reshape(35, 35)
            field[17, 33] += 1
            files["185.f32"] = field.tobytes()
            page["height_sha256"] = hashlib.sha256(files["185.f32"]).hexdigest()

        with (
            tempfile.TemporaryDirectory() as directory,
            self.assertRaisesRegex(ValueError, "shared original height words"),
        ):
            load_neighborhood(self.modified(directory, mismatch))


if __name__ == "__main__":
    unittest.main()
