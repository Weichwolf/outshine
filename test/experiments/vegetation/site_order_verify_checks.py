"""Missing/duplicated/changed attempts and stale artifact proofs cannot pass the audit."""

import copy
import hashlib
import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from site_order_verify import check_prefix, read


class PrefixChecks(unittest.TestCase):
    def test_complete_prefix_and_rejected_counterexamples(self):
        expected = [
            (2.0, "a", (0,), np.array((0, 0))),
            (3.0, "a", (1,), np.array((1, 0))),
            (4.0, "b", (2,), np.array((2, 0))),
        ]
        evidence = {
            "ordered_proposals": [
                {
                    "source_part": r[1],
                    "site_id": list(r[2]),
                    "root_xy_m": r[3].tolist(),
                    "distance_m": r[0],
                }
                for r in expected
            ]
        }
        ledger = [
            {
                "id": "a",
                "sites": [
                    {"site_id": [0], "root_xy_m": [0, 0], "state": "deferred"},
                    {
                        "site_id": [1],
                        "root_xy_m": [1, 0],
                        "state": "prepared",
                        "plant_index": 0,
                    },
                ],
            }
        ]
        records = [{"source_part_id": "a", "result": {"site_id": [1]}}]
        with tempfile.TemporaryDirectory(
            prefix="outshine-vegetation-prefix-", dir="/tmp"
        ) as temporary:
            folder = Path(temporary)
            path = folder / "landcover-ledger.json"
            path.write_text(json.dumps(ledger))
            check_prefix(
                folder, records, {"budget": {"attempts": 2}}, evidence, expected
            )
            invalid = []
            missing = copy.deepcopy(ledger)
            missing[0]["sites"].pop(0)
            invalid.append(missing)
            duplicate = copy.deepcopy(ledger)
            duplicate[0]["sites"][1] = duplicate[0]["sites"][0]
            invalid.append(duplicate)
            reversed_sites = copy.deepcopy(ledger)
            reversed_sites[0]["sites"].reverse()
            invalid.append(reversed_sites)
            moved = copy.deepcopy(ledger)
            moved[0]["sites"][1]["root_xy_m"] = [2, 0]
            invalid.append(moved)
            unknown = copy.deepcopy(ledger)
            unknown[0]["sites"][0]["site_id"] = [99]
            invalid.append(unknown)
            for changed in invalid:
                path.write_text(json.dumps(changed))
                with self.assertRaises(ValueError):
                    check_prefix(
                        folder, records, {"budget": {"attempts": 2}}, evidence, expected
                    )

    def test_changed_payload_is_rejected_after_a_matching_receipt(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-vegetation-receipt-", dir="/tmp"
        ) as temporary:
            folder = Path(temporary)
            path = folder / "plants.json"
            path.write_text("[]")
            report = {
                "commit": "test-source",
                "artifacts": {
                    "plants.json": hashlib.sha256(path.read_bytes()).hexdigest()
                },
            }
            (folder / "comparison.json").write_text(json.dumps(report))
            proof = {
                "report_sha256": hashlib.sha256(
                    (folder / "comparison.json").read_bytes()
                ).hexdigest(),
                "producer_commit": "test-source",
                "whole_form_replayed_exact": True,
                "all_physical_bounds_and_SITE_products_verified": True,
            }
            receipt = folder / "receipt.json"
            receipt.write_text(json.dumps(proof))
            self.assertEqual(read(folder, receipt)[1], [])
            path.write_text("[{}]")
            with self.assertRaises(ValueError):
                read(folder, receipt)


if __name__ == "__main__":
    unittest.main()
