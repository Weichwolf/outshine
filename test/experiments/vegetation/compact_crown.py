"""Experimental complete continuous original-host parameters, not a native format."""

import json

import numpy as np
from compact_crown_kernel import evaluate
from crown_demand import host_bounds
from outshine_ref.vegetation.curve_frame import arc_frame


class CompactCrown:
    def __init__(self, identity, data):
        self.identity = identity
        self.data = dict(data)
        for value in self.data.values():
            value.setflags(write=False)

    @classmethod
    def build(cls, source):
        crown = source.crown
        origins = np.empty((len(crown.hosts), 3))
        frames = np.empty((len(crown.hosts), 3, 3))
        phases = np.empty(len(crown.hosts))
        for i, node in enumerate(crown.hosts):
            origins[i], frames[i] = crown.stem(int(node))
            phases[i] = np.random.default_rng(crown.seed ^ int(node)).uniform(
                -np.pi, np.pi
            )
        data = {
            "hosts": crown.hosts.astype(np.uint32),
            "offsets": crown.offsets.astype(np.uint64),
            "origins": origins,
            "frames": frames,
            "phases": phases,
            "variants": crown.variants,
            "petiole": np.asarray(crown.petiole_m),
        }
        data["lower"], data["upper"] = host_bounds(source)
        for count in np.unique(crown.counts):
            if not count:
                continue
            internode = crown.length_m * 0.9 / max(0.5, int(count) - 0.5)
            t = 0.95 - np.arange(int(count)) * internode / crown.length_m
            data[f"arc{int(count)}"] = np.stack(
                arc_frame(crown.length_m, crown.bend_rad, 0, t)
            )
        return cls(source.identity, data)

    def quantized(self):
        return type(self)(
            self.identity,
            {
                key: value.astype(np.float32)
                if value.dtype.kind == "f" and key not in ("lower", "upper")
                else value
                for key, value in self.data.items()
            },
        )

    def evaluate(self, selected, *, max_leaves, anchors=False, batch_hosts=256):
        return evaluate(
            self.data,
            selected,
            max_leaves=max_leaves,
            anchors=anchors,
            batch_hosts=batch_hosts,
        )

    def save(self, path):
        metadata = np.asarray(
            json.dumps({"format": "compact-original-crown-1", "source": self.identity})
        )
        np.savez(path, metadata=metadata, **self.data)

    @classmethod
    def load(cls, path, *, expected_source):
        with np.load(path, allow_pickle=False) as z:
            meta = json.loads(str(z["metadata"]))
            if meta != {
                "format": "compact-original-crown-1",
                "source": expected_source,
            }:
                raise ValueError(
                    "same declared original-source compact product required"
                )
            data = {key: z[key] for key in z.files if key != "metadata"}
        return cls(expected_source, data)
