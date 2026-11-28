"""Stable species draws from prepared, linearly varying physical-height weights."""

import hashlib
from dataclasses import dataclass

import numpy as np

from .plan_arrays import immutable_array


def species_site_uniforms(keys, seed):
    """BLAKE2b-64 over a versioned seed/arity/little-endian integer site key.

    The upper 53 digest bits give [0,1) variates, not unique object identities.
    Caller retains the original site IDs; row order/count are not hash inputs.
    """
    keys = np.asarray(keys)
    if (
        keys.ndim != 2
        or keys.dtype.kind not in "iu"
        or keys.shape[0] > 1000000
        or not 1 <= keys.shape[1] <= 16
        or np.any(keys < 0)
        or type(seed) is not int
        or not 0 <= seed < 2**64
    ):
        raise ValueError(
            "bounded nonnegative integer site rows and uint64 seed required"
        )
    prefix = (
        b"outshine-species-1\0"
        + seed.to_bytes(8, "little")
        + keys.shape[1].to_bytes(2, "little")
    )
    little = np.asarray(keys, dtype="<u8")
    return np.fromiter(
        (
            (
                int.from_bytes(
                    hashlib.blake2b(prefix + row.tobytes(), digest_size=8).digest(),
                    "little",
                )
                >> 11
            )
            * 2**-53
            for row in little
        ),
        dtype=np.float64,
        count=len(keys),
    )


@dataclass(frozen=True, eq=False)
class SpeciesHeightMixture:
    taxa: tuple[str, ...]
    heights: np.ndarray
    cumulative: np.ndarray

    def __post_init__(self):
        h, c = np.asarray(self.heights), np.asarray(self.cumulative)
        if (
            not isinstance(self.taxa, tuple)
            or not self.taxa
            or len(self.taxa) > 2048
            or any(not isinstance(t, str) or not t for t in self.taxa)
            or tuple(sorted(set(self.taxa))) != self.taxa
        ):
            raise ValueError("bounded unique sorted taxon IDs required")
        if (
            h.ndim != 1
            or not 2 <= len(h) <= 4096
            or h.dtype.kind not in "fiu"
            or not np.isfinite(h).all()
            or np.any(abs(h.astype(np.float64)) > 1000000)
            or np.any(np.diff(h.astype(np.float64)) <= 0)
        ):
            raise ValueError(
                "strictly increasing finite Earth-height knots within 1000000m required"
            )
        if (
            c.shape != (len(h), len(self.taxa))
            or c.size > 1048576
            or c.dtype.kind not in "fiu"
            or not np.isfinite(c).all()
            or np.any(c < 0)
            or np.any(c > 1)
            or np.any(np.diff(c.astype(np.float64), axis=1) < 0)
        ):
            raise ValueError(
                "bounded nonnegative nondecreasing prepared cumulative weights required"
            )
        object.__setattr__(
            self, "heights", immutable_array(np.asarray(h, dtype=np.float64))
        )
        object.__setattr__(
            self, "cumulative", immutable_array(np.asarray(c, dtype=np.float64))
        )

    @classmethod
    def prepare(cls, taxa, heights, weights):
        taxa, h, w = tuple(taxa), np.asarray(heights), np.asarray(weights)
        if (
            w.ndim != 2
            or h.ndim != 1
            or w.shape != (len(h), len(taxa))
            or not 1 <= len(taxa) <= 2048
            or w.size > 1048576
            or w.dtype.kind not in "fiu"
            or not np.isfinite(w).all()
            or np.any(w < 0)
        ):
            raise ValueError(
                "bounded finite nonnegative height/species weights required"
            )
        order = sorted(range(len(taxa)), key=taxa.__getitem__)
        w = np.asarray(w[:, order], dtype=np.float64)
        maximum = float(w.max(initial=0))
        if maximum:
            w /= maximum
        c = np.cumsum(w, axis=1)
        maximum = float(c.max(initial=0))
        if maximum:
            c /= maximum
        return cls(tuple(taxa[i] for i in order), h, c)

    def recipe(self):
        return {
            "format": "height-species-mixture-1",
            "taxa": self.taxa,
            "heights_m": self.heights.tolist(),
            "cumulative": self.cumulative.tolist(),
        }

    @classmethod
    def from_recipe(cls, recipe):
        if recipe["format"] != "height-species-mixture-1":
            raise ValueError("explicit prepared mixture version required")
        return cls(tuple(recipe["taxa"]), recipe["heights_m"], recipe["cumulative"])

    def choose(self, height_m, uniform):
        h, u = np.broadcast_arrays(height_m, uniform)
        if (
            h.size > 1000000
            or h.dtype.kind not in "fiu"
            or u.dtype.kind not in "fiu"
            or not np.isfinite(h).all()
            or not np.isfinite(u).all()
            or np.any(u < 0)
            or np.any(u >= 1)
        ):
            raise ValueError("bounded finite heights and [0,1) variates required")
        h = np.clip(h.ravel(), self.heights[0], self.heights[-1])
        row = np.minimum(
            np.searchsorted(self.heights, h, side="right") - 1, len(self.heights) - 2
        )
        t = (h - self.heights[row]) / (self.heights[row + 1] - self.heights[row])
        total = (1 - t) * self.cumulative[row, -1] + t * self.cumulative[row + 1, -1]
        threshold = np.minimum(u.ravel() * total, np.nextafter(total, 0))
        lo, hi = (
            np.zeros(len(h), dtype=np.int64),
            np.full(len(h), len(self.taxa), dtype=np.int64),
        )
        for _ in range(len(self.taxa).bit_length()):
            mid = np.minimum((lo + hi) // 2, len(self.taxa) - 1)
            cdf = (1 - t) * self.cumulative[row, mid] + t * self.cumulative[
                row + 1, mid
            ]
            advance = threshold >= cdf
            lo, hi = np.where(advance, mid + 1, lo), np.where(advance, hi, mid)
        selected = lo
        # Missing support remains explicit; it never becomes a uniform fallback.
        return np.where(total > 0, selected, -1).reshape(u.shape)
