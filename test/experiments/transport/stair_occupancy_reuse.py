"""Reuse verified immutable prefix planes; new road facets alone need fitting."""

import json
from pathlib import Path

from camera_height_inputs import digest
from stair_occupancy_inputs import road_volumes


def prefix(report, receipt, manifest):
    if (
        not receipt.get("valid_saved_occupancy")
        or report["frame"] != manifest["frame"]
        or report["root_resident_sha256"] != manifest["root_resident_sha256"]
        or report["source_ids"] != manifest["source_ids"][: len(report["source_ids"])]
        or report["parent_reports"]
        != manifest["parent_reports"][: len(report["parent_reports"])]
        or report.get("borrowed_headroom", {}).get("records")
    ):
        raise ValueError("verified unchanged physical-only occupancy prefix required")


def reused(path, assets, sources, manifest):
    # Imported at call time: the independent reader also imports the experiment.
    from stair_occupancy_verify import read

    path = Path(path)
    report = json.loads((path / "comparison.json").read_text())
    receipt = json.loads((path / "verification.json").read_text())
    if receipt["products_report_sha256"] != digest(path / "comparison.json"):
        raise ValueError("prior occupancy receipt differs from the saved product")
    prefix(report, receipt, manifest)
    rows, metadata = read(path / "volumes.json", report)
    keys = tuple(tuple(r["source_ids"]) for r in metadata)
    n = report["actual_road_faces"]
    support_count = report["actual_support_cap_faces"]
    if len(rows) != n + support_count or receipt["verified_actual_facets"] != len(rows):
        raise ValueError("complete verified prefix facet ledger required")
    old = set(report["source_ids"])
    retained, added = [], []
    offset = 0
    for asset in assets:
        if set(asset.source_ids) <= old:
            retained.append(asset)
            for face in range(len(asset.data["top_faces"])):
                if (
                    offset >= n
                    or rows[offset].owner_id != f"{asset.identity}:face:{face}"
                ):
                    raise ValueError("prior physical face identity/order changed")
                actual = tuple(
                    k
                    for k, used in zip(
                        asset.source_ids, asset.data["owners"][face], strict=True
                    )
                    if used
                )
                if keys[offset] != actual:
                    raise ValueError("prior physical source ownership changed")
                offset += 1
        elif set(asset.source_ids) & old:
            raise ValueError("prior source prefix splits an immutable asset")
        else:
            added.append(asset)
    support_prefix = manifest["support_sha256"] + ":support:"
    if offset != n or any(not r.owner_id.startswith(support_prefix) for r in rows[n:]):
        raise ValueError("prior road/support physical prefix incomplete")
    fresh, owners, error = road_volumes(added, sources)
    return (
        (*rows[:n], *fresh, *rows[n:]),
        (*keys[:n], *owners, *keys[n:]),
        max(error, report["cap_outward_padding_bound_m"]),
        {
            "method": "verified_prefix",
            "prior_report_sha256": digest(path / "comparison.json"),
            "reused_physical_volumes": len(rows),
            "fitted_physical_volumes": len(fresh),
            "prior_serialized_volume_bytes": (path / "volumes.json").stat().st_size,
            "retained_assets": len(retained),
            "new_assets": len(added),
        },
    )
