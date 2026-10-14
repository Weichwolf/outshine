"""Bounded pose packets from complete plans; retain no previous shoot detail."""

from pathlib import Path

import numpy as np
from outshine_ref.vegetation.plan_arrays import detail_budget


class PosePackets:
    def __init__(self, directory, name, capacity=16384):
        detail_budget(capacity)
        if capacity == 0:
            raise ValueError("positive pose packet capacity required")
        self.directory, self.name, self.capacity = Path(directory), name, capacity
        self.buffers = {
            "origins": np.empty((capacity, 3), np.float64),
            "frames": np.empty((capacity, 3, 3), np.float64),
            "ids": np.empty(capacity, np.uint64),
            "scales": np.empty(capacity, np.float64),
        }
        self.used, self.total, self.records = 0, 0, []

    def append(self, origins, frames, ids, scales):
        values = dict(origins=origins, frames=frames, ids=ids, scales=scales)
        count = len(ids)
        if any(
            v.shape != (count, *self.buffers[k].shape[1:]) for k, v in values.items()
        ):
            raise ValueError("matching pose packet fields required")
        begin = 0
        while begin < count:
            rows = min(count - begin, self.capacity - self.used)
            for key, data in values.items():
                self.buffers[key][self.used : self.used + rows] = data[
                    begin : begin + rows
                ]
            self.used += rows
            begin += rows
            if self.used == self.capacity:
                self.flush()

    def flush(self):
        if not self.used:
            return
        name = f"{self.name}-{len(self.records)}.npz"
        np.savez_compressed(
            self.directory / name,
            **{k: v[: self.used] for k, v in self.buffers.items()},
        )
        self.records.append(
            {
                "placements": name,
                "instances": self.used,
                "first_id": int(self.buffers["ids"][0]),
                "last_id": int(self.buffers["ids"][self.used - 1]),
            }
        )
        self.total += self.used
        self.used = 0

    def finish(self):
        self.flush()
        return tuple(self.records)


def stream_foliage(plan, directory, prefix):
    needles = [
        PosePackets(directory, f"{prefix}-needles-{i}")
        for i in range(len(plan.needles))
    ]
    shorts = [
        PosePackets(directory, f"{prefix}-shorts-{i}")
        for i in range(len(plan.patterns))
    ]
    maximum = 0
    for path in plan.wood.paths():
        group = plan.foliage(path, 100000, retain=False)
        if group is None:
            continue
        maximum = max(maximum, len(group["ids"]))
        for index, packets in enumerate(needles):
            choose = group["variants"] == index
            packets.append(
                *(group[k][choose] for k in ("origins", "frames", "ids", "scales"))
            )
        if plan.patterns[len(path)].short_length:
            shorts[len(path)].append(
                group["short_origins"],
                group["short_frames"],
                group["short_ids"],
                np.ones(len(group["short_ids"])),
            )
    needle_records = [p.finish() for p in needles]
    short_records = [p.finish() for p in shorts]
    if sum(p.total for p in needles) != sum(plan.needle_counts) or plan._groups:
        raise AssertionError(
            "complete streamed poses without retained shoot details required"
        )
    return (
        needle_records,
        short_records,
        {
            "max_detail_rows": maximum,
            "pose_buffer_bytes": sum(
                a.nbytes for p in (*needles, *shorts) for a in p.buffers.values()
            ),
            "needle_count": sum(p.total for p in needles),
            "short_shoot_count": sum(p.total for p in shorts),
            "retained_shoot_groups": len(plan._groups),
        },
    )
