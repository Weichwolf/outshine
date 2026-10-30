"""Append keeps the existing verified replay and bounded immutable lineage."""

import unittest
from pathlib import Path
from unittest.mock import patch

from stair_local_replay import append_verified, load_chain


class ReplayAppendChecks(unittest.TestCase):
    def setUp(self):
        self.manifest = {
            "root_resident_sha256": "root",
            "source_ids": ["old"],
            "parent_reports": [],
            "borrowed_headroom_contacts": [{"old": True}],
        }
        self.state = object(), (object(),), (), (), self.manifest
        self.report = {
            "candidate_source_ids": ["old", "new"],
            "artifacts": {"module.npz": "module-sha"},
        }
        self.path = Path("/tmp/outshine-append-fixture/module")

    def test_append_preserves_before_and_extends_exact_existing_manifest_fields(self):
        following = (object(), (*self.state[1], object()), self.state[2], self.state[3])
        with (
            patch("stair_local_replay.verified") as verified,
            patch(
                "stair_local_replay.apply_product",
                return_value=(*following, self.manifest, self.report, None),
            ) as apply,
            patch("stair_local_replay.digest", side_effect=lambda p: "hash-" + p.name),
            patch("stair_local_replay.declarations", return_value=[{"new": True}]),
        ):
            result = append_verified(self.state, self.path)
        verified.assert_called_once_with(self.path)
        apply.assert_called_once_with(self.state, self.path)
        self.assertEqual(result[:4], following)
        self.assertEqual(result[4]["source_ids"], ["old", "new"])
        self.assertEqual(result[4]["parent_reports"], ["hash-comparison.json"])
        self.assertEqual(
            result[4]["borrowed_headroom_contacts"], [{"old": True}, {"new": True}]
        )
        self.assertEqual(self.manifest["source_ids"], ["old"])
        self.assertEqual(self.manifest["parent_reports"], [])
        self.assertEqual(self.manifest["borrowed_headroom_contacts"], [{"old": True}])

    def test_module_capacity_rejects_before_receipt_or_array_work(self):
        state = (*self.state[:4], dict(self.manifest, parent_reports=["old"] * 32))
        with (
            patch(
                "stair_local_replay.verified",
                side_effect=AssertionError("no IO beyond capacity"),
            ),
            self.assertRaisesRegex(ValueError, "capacity"),
        ):
            append_verified(state, self.path)

    def test_failed_receipt_or_logical_capacity_cannot_publish_or_mutate_before(self):
        with (
            patch(
                "stair_local_replay.verified", side_effect=ValueError("invalid receipt")
            ),
            self.assertRaisesRegex(ValueError, "invalid receipt"),
        ):
            append_verified(self.state, self.path)
        with (
            patch("stair_local_replay.verified"),
            patch(
                "stair_local_replay.apply_product",
                return_value=(*self.state[:4], self.manifest, self.report, None),
            ),
            patch("stair_local_replay.digest", return_value="hash"),
            patch("stair_local_replay.declarations", return_value=[{}] * 128),
            self.assertRaisesRegex(ValueError, "headroom declaration budget"),
        ):
            append_verified(self.state, self.path)
        self.assertEqual(self.manifest["parent_reports"], [])
        self.assertEqual(len(self.manifest["borrowed_headroom_contacts"]), 1)

    def test_full_chain_uses_the_same_append_in_order_and_retains_input_limit(self):
        second, third = object(), object()
        paths = (self.path, self.path.with_name("next"))
        with (
            patch("stair_local_replay.load", return_value=self.state),
            patch("stair_local_replay.digest", return_value="root-sha"),
            patch(
                "stair_local_replay.append_verified", side_effect=[second, third]
            ) as apply,
        ):
            result = load_chain(self.path.parent, paths)
        self.assertIs(result, third)
        self.assertEqual(apply.call_args_list[0].args[1], paths[0])
        self.assertEqual(
            apply.call_args_list[0].args[0][4]["root_resident_sha256"], "root-sha"
        )
        self.assertEqual(apply.call_args_list[1].args, (second, paths[1]))
        with patch(
            "stair_local_replay.load",
            side_effect=AssertionError("must reject before load"),
        ):
            for prior in (
                (self.path, self.path),
                tuple(self.path / str(i) for i in range(33)),
            ):
                with self.assertRaisesRegex(ValueError, "bounded unique"):
                    load_chain(self.path.parent, prior)


if __name__ == "__main__":
    unittest.main()
