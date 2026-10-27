"""Acquisition-time reference products from the declared finite WATER3 snapshot."""

import dataclasses
import json
from collections import defaultdict

import numpy as np
from compare_canonical_water_domains import observed_snapshots
from compare_water_owner_inputs import course_contents, layer_receipts
from native_water_observation import NO_INPUT
from outshine_ref.places.camera import ecef
from outshine_ref.places.sources import tile_geography
from water_colored_updates import UpdatedNode
from water_course_envelope import course_block, summarize
from water_envelope_query import EnvelopeIndex


def acquire(path):
    data, raw, _ = observed_snapshots(path)
    snapshots = {t: dataclasses.replace(s, complete=True) for t, s in raw.items()}
    epoch = tuple(next(iter(raw.values())).source[k] for k in ("provider", "revision"))
    receipts, order = layer_receipts(data, snapshots, epoch)
    return data, snapshots, epoch, receipts, order


def source_blocks(data, snapshots, receipts):
    parts = {key: {} for key in snapshots}
    aliases, omitted = defaultdict(list), defaultdict(list)
    for index, course in enumerate(data["courses"]):
        if int(course["input"]) == NO_INPUT:
            omitted["missing_provenance"].append(index)
            continue
        info = data["inputs"][int(course["input"])]
        if info["kind"] != "river" or info["layer"] != "water_lines":
            omitted["other_course_class"].append(index)
            continue
        if (
            info.get("bridge") is not False
            or info.get("tunnel") is not False
            or info.get("crossing")
        ):
            omitted["not_ground"].append(index)
            continue
        tile, identity, digest = course_contents(data, index)
        first, count = int(course["first_point"]), int(course["point_count"])
        points = data["points_lat_lon"][first : first + count]
        world = ecef(points[:, 0], points[:, 1], 0.0)
        levels = data["levels"][
            int(course["first_level"]) : int(course["first_level"]) + count
        ]
        for segment in range(count - 1):
            if np.array_equal(world[segment], world[segment + 1]):
                omitted["zero_length_segment"].append((index, segment))
                continue
            key = json.dumps([identity, digest, segment], separators=(",", ":"))
            parts[tile][key] = (
                world[segment : segment + 2],
                levels[segment : segment + 2],
            )
            aliases[(tile, key)].append((index, segment))
    blocks = {}
    for tile, entries in parts.items():
        ids = sorted(entries)
        blocks[tile] = course_block(
            tile,
            [entries[i][0] for i in ids],
            [entries[i][1] for i in ids],
            ids,
            source_version=receipts[(tile, "water_lines")].digest,
        )
    summaries = {
        tile: summarize(block, complete=receipts[(tile, "water_lines")].complete)
        for tile, block in blocks.items()
    }
    return (
        blocks,
        summaries,
        {
            "omitted_original_courses_or_segments": dict(omitted),
            "original_aliases": [
                {"tile": tile, "id": identity, "original_addresses": addresses}
                for (tile, identity), addresses in aliases.items()
            ],
        },
    )


class CourseParent:
    def __init__(self, index, blocks):
        self.index, self.blocks = index, blocks
        self.version = index.version
        self.queries, self.nodes = {}, {}
        self.loads = []

    def load(self, key):
        self.loads.append(key)
        return self.blocks.get(key)

    def __call__(self, key):
        if key not in self.nodes:
            if len(self.nodes) >= 512:
                return UpdatedNode("budget_exhausted", None, None)
            z, x, y = key
            lat, lon = tile_geography([[x / 2, y / 2]], 1, (z, 0, 0))
            result = self.index.query(ecef(lat[0], lon[0], 0.0), self.load)
            self.queries[key] = result
            world = (
                ecef(lat[0], lon[0], result.height_m)
                if result.state == "ready"
                else None
            )
            if world is not None:
                world.setflags(write=False)
            self.nodes[key] = UpdatedNode(result.state, world, None)
        return self.nodes[key]


def prepare_parent(data, snapshots, receipts):
    blocks, summaries, provenance = source_blocks(data, snapshots, receipts)
    index = EnvelopeIndex(
        tuple(snapshots),
        summaries,
        scope="declared-finite-WATER3-ground-river-source-layer-v1",
    )
    return CourseParent(index, blocks), provenance
