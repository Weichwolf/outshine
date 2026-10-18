"""Real child-process failure controls and bounded recipe continuation."""

import json
import tempfile
import time
import unittest
from pathlib import Path
from types import SimpleNamespace

from stair_batch_experiment import isolated
from stair_batch_inputs import candidate, identity, limits


class BoundedBatchControls(unittest.TestCase):
    def test_real_timeout_kills_worker_without_publishing_success(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-transport-batch-test-"
        ) as tmp:
            root = Path(tmp)
            worker = root / "worker.py"
            worker.write_text(
                "import time\nprint('started',flush=True)\ntime.sleep(10)\nprint('finished',flush=True)\n"
            )
            began = time.perf_counter()
            result = isolated(root, 0.4, worker=worker)
            self.assertEqual(result["status"], "timeout")
            self.assertLess(time.perf_counter() - began, 5)
            self.assertEqual((root / "process.log").read_text(), "started\n")
            self.assertFalse((root / "step.json").exists())

    def test_zero_exit_without_fresh_export_is_failure(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-transport-batch-test-"
        ) as tmp:
            root = Path(tmp)
            worker = root / "worker.py"
            worker.write_text("pass\n")
            with self.assertRaisesRegex(ValueError, "fresh complete export"):
                isolated(root, 5, worker=worker)

    def test_nonzero_child_exit_cannot_publish_a_claimed_success(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-transport-batch-test-"
        ) as tmp:
            root = Path(tmp)
            worker = root / "worker.py"
            worker.write_text(
                "import pathlib,sys\npathlib.Path(sys.argv[1], 'step.json').write_text('{\"status\":\"accepted\"}')\nsys.exit(7)\n"
            )
            self.assertEqual(
                isolated(root, 5, worker=worker),
                {"status": "process_error", "returncode": 7},
            )

    def test_recipe_identity_retains_real_alternatives_and_skips_exact_repeats(self):
        a = {"seed": {"end": 0, "source_id": "a"}, "source_ids": ["a", "b"]}
        same = json.loads('{"source_ids":["a","b"],"seed":{"source_id":"a","end":0}}')
        prefix = {**a, "source_ids": ["a"]}
        other_end = {**a, "seed": {"source_id": "a", "end": -1}}
        selected = {"recipes": [a, same, prefix, other_end]}
        self.assertEqual(identity(a), identity(same))
        self.assertEqual(candidate(selected, {identity(a)}), (2, prefix))
        self.assertEqual(
            candidate(selected, {identity(a), identity(prefix)}), (3, other_end)
        )
        self.assertIsNone(
            candidate(selected, {identity(a), identity(prefix), identity(other_end)})
        )

    def test_invalid_limits_and_half_pinned_inputs_rejected(self):
        good = dict(
            max_attempts=8,
            seconds=300,
            attempt_seconds=60,
            prepared_reference=None,
            reference_sha256=None,
        )
        for key, value in (
            ("max_attempts", 0),
            ("max_attempts", 9),
            ("max_attempts", True),
            ("seconds", float("nan")),
            ("seconds", float("inf")),
            ("attempt_seconds", 0),
            ("attempt_seconds", True),
            ("prepared_reference", Path("missing")),
        ):
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                limits(SimpleNamespace(**{**good, key: value}))
        limits(SimpleNamespace(**good))


if __name__ == "__main__":
    unittest.main()
