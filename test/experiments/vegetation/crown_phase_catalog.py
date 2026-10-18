"""Bounded shared source-shoot representatives; exact source IDs remain on the plan."""

import hashlib
import json

import numpy as np
from outshine_ref.vegetation.plan_arrays import immutable_array


class CrownPhaseCatalog:
    def __init__(self, source, bins=32, max_templates=512):
        if type(bins) is not int or bins not in (8, 16, 32, 64):
            raise ValueError("declared8/16/32/64 phase bins required")
        if type(max_templates) is not int or not 0 <= max_templates <= 4096:
            raise ValueError("bounded complete prototype capacity required")
        crown = source.crown
        families = {}
        keys = []
        phases = []
        for i, node in enumerate(crown.hosts):
            first, last = map(int, crown.offsets[i : i + 2])
            sequence = tuple(map(int, crown.variants[first:last]))
            family = families.setdefault(sequence, len(families))
            phase = float(
                np.random.default_rng(crown.seed ^ int(node)).uniform(-np.pi, np.pi)
            )
            phase_bin = min(
                bins - 1, int(np.floor((phase + np.pi) * bins / (2 * np.pi)))
            )
            keys.append((family, phase_bin))
            phases.append(phase)
        groups = {}
        for i, key in enumerate(keys):
            groups.setdefault(key, []).append(i)
        if len(groups) > max_templates:
            raise ValueError(
                "complete phase catalog exceeds capacity before source poses"
            )
        representatives = []
        mapping = np.empty(len(keys), np.uint16)
        errors = []
        phases = np.asarray(phases)
        for key, indices in sorted(groups.items()):
            centre = -np.pi + (key[1] + 0.5) * 2 * np.pi / bins
            distance = np.abs(np.angle(np.exp(1j * (phases[indices] - centre))))
            chosen = indices[int(np.argmin(distance))]
            mapping[indices] = len(representatives)
            representatives.append(int(crown.hosts[chosen]))
            errors.extend(
                np.abs(np.angle(np.exp(1j * (phases[indices] - phases[chosen]))))
            )
        self.source = source
        self.bins = bins
        self.representatives = tuple(representatives)
        self.mapping = immutable_array(mapping)
        self.max_phase_error_rad = float(max(errors, default=0))
        self.families = len(families)
        self.templates = {}

    def recipe(self):
        return {
            "format": "spatial-crown-phase-catalog-1",
            "source_identity": self.source.identity,
            "leaf_plan_identity": self.source.crown.identity,
            "phase_bins": self.bins,
            "representative_host_nodes": self.representatives,
        }

    @property
    def identity(self):
        return hashlib.sha256(
            json.dumps(self.recipe(), sort_keys=True).encode() + self.mapping.tobytes()
        ).hexdigest()

    def geometry(self, node, max_leaves=128):
        if type(max_leaves) is not int or not 0 <= max_leaves <= 128:
            raise ValueError("bounded explicit source group capacity required")
        crown = self.source.crown
        i = crown._host_index(node)
        count = int(crown.counts[i])
        if count > max_leaves:
            return None
        slot = int(self.mapping[i])
        if slot not in self.templates:
            self.templates[slot] = self.source.geometry(
                self.representatives[slot], max_leaves
            )
            for value in self.templates[slot].values():
                if isinstance(value, np.ndarray):
                    value.setflags(write=False)
        template = self.templates[slot]
        first, last = map(int, crown.offsets[i : i + 2])
        np.testing.assert_array_equal(
            template["leaf_variants"], crown.variants[first:last]
        )
        origin, frame = crown.stem(node)
        ids = np.arange(first, last, dtype=np.uint64)
        ids.setflags(write=False)
        return template | {
            "ids": ids,
            "anchor_origin": origin,
            "anchor_frame": frame,
            "host_node": node,
            "source_representative_node": self.representatives[slot],
            "representation_identity": self.identity,
        }

    def summary(self):
        crown = self.source.crown
        count = sum(
            int(crown.counts[crown._host_index(n)]) for n in self.representatives
        )
        return {
            "identity": self.identity,
            "hosts": len(crown.hosts),
            "leaves": len(crown.variants),
            "families": self.families,
            "templates": len(self.representatives),
            "representative_pose_rows": count,
            "phase_bins": self.bins,
            "maximum_phase_difference_rad": self.max_phase_error_rad,
            "mapping_array_bytes": self.mapping.nbytes,
            "prepared_templates": len(self.templates),
        }
