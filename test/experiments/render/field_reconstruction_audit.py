"""Observe the unchanged reconstruction decisions; no relaxed field is produced."""

import numpy as np
from reconstructed_source_field import CORNER_IDS

ZERO = 1
POSITIVE_CORNERS = 2
VALUE_FIT = 4
PROBE_SIGNS = 8


class ReconstructionAudit:
    def __init__(self):
        self.rows = []
        self.wrong_probes = []

    def __call__(self, builder, record, sampled, error, probe_value, reconstructed):
        node, coordinate, width, level, candidates, probes = record
        side = width * builder.unit
        corners = sampled[CORNER_IDS].astype(np.float32)
        truth = builder.probe_values[probes]
        wrong = (abs(truth) > 1e-8) & ((probe_value <= 0) != (truth <= 0))
        mask = (
            ZERO * int(np.all(corners == 0))
            | POSITIVE_CORNERS * int(corners.min() > 0)
            | VALUE_FIT * int(error > max(1e-4, side * 0.05))
            | PROBE_SIGNS * int(wrong.any())
        )
        if bool(mask == 0) != bool(reconstructed):
            raise ValueError("audit predicates disagree with the actual builder")
        depth_stop = level >= builder.depth
        budget_stop = len(builder.tags) + 8 > builder.max_nodes
        terminal = int(reconstructed) or (3 if depth_stop else 2 if budget_stop else 0)
        self.rows.append(
            (
                node,
                level,
                side,
                terminal,
                mask,
                error,
                len(probes),
                int(wrong.sum()),
                int((wrong & (truth < 0)).sum()),
                len(np.unique(builder.owners[candidates])),
                *(builder.low + coordinate * builder.unit),
            )
        )
        if wrong.any():
            self.wrong_probes.append(
                np.column_stack(
                    (
                        np.full(int(wrong.sum()), node),
                        probes[wrong],
                        truth[wrong],
                        probe_value[wrong],
                    )
                )
            )

    def arrays(self):
        return {
            "decisions": np.asarray(self.rows, float).reshape(-1, 13),
            "wrong_probes": np.concatenate(self.wrong_probes)
            if self.wrong_probes
            else np.empty((0, 4)),
        }

    def summary(self):
        rows = self.arrays()["decisions"]
        unknown = rows[(rows[:, 3] == 2) | (rows[:, 3] == 3)]
        masks = unknown[:, 4].astype(int)
        return {
            "fit_attempts": len(rows),
            "unknown_nodes": len(unknown),
            "node_budget_stops": int((unknown[:, 3] == 2).sum()),
            "minimum_side_stops": int((unknown[:, 3] == 3).sum()),
            "unknown_by_side_m": {
                str(side): int((unknown[:, 2] == side).sum())
                for side in np.unique(unknown[:, 2])
            },
            "overlapping_failure_counts": {
                name: int(((masks & bit) != 0).sum())
                for name, bit in (
                    ("zero_corners_without_containment", ZERO),
                    ("all_positive_corners", POSITIVE_CORNERS),
                    ("exterior_value_fit", VALUE_FIT),
                    ("source_probe_signs", PROBE_SIGNS),
                )
            },
            "failure_mask_counts": {
                str(mask): int((masks == mask).sum()) for mask in np.unique(masks)
            },
            "would_pass_only_if_sign_requirement_removed": int(
                (masks == PROBE_SIGNS).sum()
            ),
            "scope": "Overlapping finite predicates at unchanged decisions. Removing a predicate is not a valid field or an image/error qualification; no counterfactual rendering or parameter sweep.",
        }
