"""Measure irrelevant opaque-blocked GGX; keep the actual renderer unchanged."""

import json
from unittest.mock import patch

import building_tree_transfer as layers
import compare_building_parent_shading as comparison
import numpy as np
from building_parent_shading_work import opaque_blocked


class LayerWorkProbe:
    def __init__(self):
        self.actual_shade = layers.shade
        self.actual_compose = layers.compose_layers
        self.actual_transfer = layers.transfers
        self.actual_run = comparison.run
        self.capture, self.results = [], []
        self.last = None

    def shade(self, *args, **kwargs):
        value, work = self.actual_shade(*args, **kwargs)
        _, weights, _, normal, outgoing, light, _ = args
        normal, outgoing = [np.asarray(v, float) for v in (normal, outgoing)]
        light = np.array(light, float)
        light /= np.linalg.norm(light)
        lit = (normal @ light > 0) & (np.einsum("ij,ij->i", normal, outgoing) > 0)
        per_row = np.count_nonzero(weights > 0, axis=1) * lit
        if kwargs.get("intensity", 3) == 0:
            per_row[:] = 0
        if per_row.sum() != work["ggx_lobe_evaluations"]:
            raise ValueError("diagnostic lobe counts must match actual shading")
        self.capture.append(per_row)
        return value, work

    def compose(self, rays, depth, colour, opacity, ties, count):
        result = self.actual_compose(rays, depth, colour, opacity, ties, count)
        lobe = np.concatenate(self.capture) if self.capture else np.empty(0, int)
        if len(lobe) != len(rays):
            raise ValueError("one actual shading record per composed layer required")
        blocked = opaque_blocked(rays, depth, opacity, ties)
        keep = ~blocked
        shorter = self.actual_compose(
            rays[keep], depth[keep], colour[keep], opacity[keep], ties[keep], count
        )
        for expected, actual in zip(result[:2], shorter[:2], strict=True):
            if (
                actual.dtype != expected.dtype
                or actual.shape != expected.shape
                or actual.tobytes() != expected.tobytes()
            ):
                raise ValueError("blocked layers must have exactly zero C/T effect")
        self.last = {
            "input_layers": len(rays),
            "opaque_layers": int(np.count_nonzero(opacity == 1)),
            "fully_blocked_layers": int(np.count_nonzero(blocked)),
            "input_ggx": int(lobe.sum()),
            "fully_blocked_ggx": int(lobe[blocked].sum()),
            "exact_dropped_layer_C_T": True,
        }
        return result

    def transfer(self, *args, **kwargs):
        self.capture, self.last = [], None
        image, work = self.actual_transfer(*args, **kwargs)
        if self.last is None:
            raise ValueError("completed composition diagnostic required")
        work["ineffective_layer_diagnostic"] = self.last
        self.results.append({"rate": args[8], "work": self.last})
        return image, work

    def run(self, args):
        self.actual_run(args)
        report = {
            "comparison_report": "report.json",
            "comparisons": self.results,
            "scope": "Original rendering unchanged. Strict alpha1/depth/face-tie ordering; per-sample exact C/T checks. Ineffective reference GGX is a count, not measured reordered work, native invocations, GPU time or a general occlusion result. Instrumentation itself sorts, copies and composes twice, with paid diagnostic costs.",
        }
        (args.output / "layer-work.json").write_text(
            json.dumps(report, indent=2) + "\n"
        )


def main():
    probe = LayerWorkProbe()
    with (
        patch.object(layers, "shade", probe.shade),
        patch.object(layers, "compose_layers", probe.compose),
        patch.object(comparison, "transfers", probe.transfer),
        patch.object(comparison, "run", probe.run),
    ):
        comparison.main()


if __name__ == "__main__":
    main()
