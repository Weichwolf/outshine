"""Transient height/normal-conditioned shadow transfer; no shaded-colour field."""

from dataclasses import dataclass

import numpy as np
from field_sampling import bilinear
from urban_parent_conditional import trace_transport
from urban_parent_phase import phase_rays


def collect_transfer(parent, origins, directions, profile, *, side=6, batch=256):
    if type(batch) is not int or not 1 <= batch <= 4096:
        raise ValueError("bounded transfer packet required")
    slots = len(parent.heights) * 5 + 1
    moments = np.zeros((len(origins), slots, 2))
    work = {
        "phase_rays": 0,
        "phase_packet_array_peak_bytes": 0,
        "transfer_bytes": moments.nbytes,
    }
    for first in range(0, len(origins), batch):
        part = slice(first, first + batch)
        rays, ray_directions, quadrature = phase_rays(
            parent, origins[part], directions[part], side
        )

        def receive(
            ids,
            slot,
            points,
            normal,
            weight,
            visibility,
            first=first,
            quadrature=quadrature,
        ):
            anchor = first + ids // side**2
            weight = weight * quadrature[ids % side**2]
            np.add.at(moments[..., 0], (anchor, slot), weight)
            np.add.at(moments[..., 1], (anchor, slot), weight * visibility)

        _, counts = trace_transport(parent, rays, ray_directions, profile, receive)
        work["phase_rays"] += len(rays)
        work["phase_packet_array_peak_bytes"] = max(
            work["phase_packet_array_peak_bytes"], rays.nbytes + ray_directions.nbytes
        )
        for key, value in counts.items():
            work[key] = (
                max(work.get(key, 0), value)
                if key.endswith("peak_bytes")
                else work.get(key, 0) + value
            )
    return moments, work


@dataclass
class ShadowTransfer:
    moments: np.ndarray
    macro_shape: tuple
    phase_side: int
    macro_first: int = 0

    def prepare(self, first, count):
        self.macro_first = first
        macro = first + np.arange(count)
        width, height = self.macro_shape
        uv = np.column_stack(
            ((macro % width + 0.5) / width, (macro // width + 0.5) / height)
        )
        image = self.moments.reshape(*self.moments.shape[:2], -1)
        samples = bilinear(image, uv).reshape(count, -1, 2)
        mass, joint = samples[..., 0], samples[..., 1]
        self.visibility = np.divide(
            joint, mass, out=np.full(mass.shape, np.nan), where=mass > 0
        )

    def __call__(self, rays, slots):
        return self.visibility[rays // self.phase_side**2, slots]
