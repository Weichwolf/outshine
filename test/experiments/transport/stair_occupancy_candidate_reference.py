"""Independent unchanged-volume partitions for the real 102-to-105 comparison."""

import json
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from stair_local_replay import load_chain
from stair_occupancy_handoff import OccupancyQuery
from stair_occupancy_inputs import road_volumes
from stair_occupancy_verify import read


class PartitionReference:
    def __init__(self, products, state, sources):
        report = json.loads((products / "comparison.json").read_text())
        receipt = json.loads((products / "verification.json").read_text())
        if not receipt["valid_saved_occupancy"] or receipt[
            "products_report_sha256"
        ] != digest(products / "comparison.json"):
            raise ValueError("independently verified baseline required")
        prefix = tuple(report["source_ids"])
        current = tuple(state[4]["source_ids"])
        if current[: len(prefix)] != prefix:
            raise ValueError("whole unchanged source prefix required")
        old, metadata = read(products / "volumes.json", report)
        baseline = load_chain(
            Path(report["resident"]), [Path(p) for p in report["prior_modules"]]
        )
        asset_count = len(baseline[1])
        for old_asset, current_asset in zip(
            baseline[1], state[1][:asset_count], strict=True
        ):
            if old_asset.identity != current_asset.identity or any(
                not np.array_equal(value, current_asset.data[name])
                for name, value in old_asset.data.items()
            ):
                raise ValueError("baseline road body changed")
        if baseline[4]["support_sha256"] != state[4]["support_sha256"] or (
            baseline[4].get("borrowed_headroom_contacts")
            != state[4].get("borrowed_headroom_contacts")
        ):
            raise ValueError("baseline support or logical contact changed")
        extra, owners, _error = road_volumes(state[1][asset_count:], sources)
        self.rows = (*old, *extra)
        self.owners = (*(tuple(r["source_ids"]) for r in metadata), *owners)
        self.by_id = {
            r.owner_id: (r, keys)
            for r, keys in zip(self.rows, self.owners, strict=True)
        }
        if len(self.by_id) != len(self.rows):
            raise ValueError("duplicate old reference identity")
        self.queries = [OccupancyQuery(old)]
        if extra:
            self.queries.append(OccupancyQuery(extra))

    def conflict(self, footprint, **kwargs):
        cuts = [query.conflict(footprint, **kwargs)[0] for query in self.queries]
        return shapely.union_all(cuts), {}
