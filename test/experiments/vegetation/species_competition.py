"""Order-independent competing spatial species marks; experimental ecology."""

import hashlib
from dataclasses import dataclass

import numpy as np
from outshine_ref.vegetation import species_site_uniforms
from species_spatial import FourierMarks


def taxon_seed(taxon, seed, role):
    if (
        not isinstance(taxon, str)
        or not 0 < len(taxon.encode("utf-8")) <= 4096
        or type(seed) is not int
        or not 0 <= seed < 2**64
        or role not in ("field", "site")
    ):
        raise ValueError(
            "bounded taxon ID, uint64 seed and explicit random role required"
        )
    message = (
        b"outshine-species-race-1\0"
        + seed.to_bytes(8, "little")
        + role.encode("ascii")
        + b"\0"
        + taxon.encode("utf-8")
    )
    return int.from_bytes(hashlib.blake2b(message, digest_size=8).digest(), "little")


def gumbel_scores(weights, uniform):
    w, u = np.asarray(weights), np.asarray(uniform)
    if (
        w.shape != u.shape
        or w.dtype.kind not in "fiu"
        or u.dtype.kind not in "fiu"
        or not np.isfinite(w).all()
        or not np.isfinite(u).all()
        or np.any(w < 0)
        or np.any((u < 0) | (u >= 1))
    ):
        raise ValueError(
            "matching finite nonnegative weights and unit variates required"
        )
    result = np.full(w.shape, -np.inf)
    active = w > 0
    bounded = np.maximum(u[active], 2**-54)
    result[active] = np.log(w[active]) - np.log(-np.log(bounded))
    return result


@dataclass(frozen=True)
class CompetingMarks:
    entries: tuple

    def __post_init__(self):
        if (
            not isinstance(self.entries, tuple)
            or not 1 <= len(self.entries) <= 128
            or any(not isinstance(e, tuple) or len(e) != 3 for e in self.entries)
        ):
            raise ValueError("one to128 explicit taxon field/seed entries required")
        names = [e[0] for e in self.entries]
        if len(set(names)) != len(names):
            raise ValueError("unique taxon entries required")
        for name, field, seed in self.entries:
            taxon_seed(name, seed, "site")
            if not isinstance(field, FourierMarks):
                raise TypeError("prepared Fourier marks required")

    @classmethod
    def prepare(cls, taxa, seed, length_m=60, count=256):
        if not 1 <= len(taxa) <= 128 or len(set(taxa)) != len(taxa):
            raise ValueError("one to128 unique taxon IDs required")
        return cls(
            tuple(
                (
                    taxon,
                    FourierMarks.prepare(
                        taxon_seed(taxon, seed, "field"), length_m, count
                    ),
                    taxon_seed(taxon, seed, "site"),
                )
                for taxon in sorted(taxa)
            )
        )

    def recipe(self):
        return {
            "format": "competing-species-marks-1",
            "entries": [
                {"taxon": name, "field": field.recipe(), "site_seed": seed}
                for name, field, seed in self.entries
            ],
        }

    @classmethod
    def from_recipe(cls, recipe):
        if recipe["format"] != "competing-species-marks-1":
            raise ValueError("explicit competing-marks version required")
        return cls(
            tuple(
                (e["taxon"], FourierMarks.from_recipe(e["field"]), e["site_seed"])
                for e in recipe["entries"]
            )
        )

    def choose(
        self, mixture, world_m, keys, height_m, strength=0.85, score_mode="stream"
    ):
        p, keys, h = np.asarray(world_m), np.asarray(keys), np.asarray(height_m)
        if (
            h.ndim != 1
            or p.shape != (len(h), 3)
            or p.dtype.kind not in "fiu"
            or not np.isfinite(p).all()
            or np.any(abs(p.astype(np.float64)) > 10000000)
            or len(h) > 1000000
            or h.dtype.kind not in "fiu"
            or not np.isfinite(h).all()
            or keys.ndim != 2
            or keys.dtype.kind not in "iu"
            or not 1 <= keys.shape[1] <= 16
            or np.any(keys < 0)
            or len(keys) != len(h)
            or not np.isfinite(strength)
            or not 0 <= strength <= 1
            or set(mixture.taxa) - {e[0] for e in self.entries}
            or score_mode not in ("stream", "matrix")
        ):
            raise ValueError(
                "matching bounded sites/heights and prepared taxon fields required"
            )
        if score_mode == "matrix" and len(h) * len(mixture.taxa) > 16777216:
            raise ValueError("reference score matrix exceeds explicit budget")
        h = np.clip(h, mixture.heights[0], mixture.heights[-1])
        row = np.minimum(
            np.searchsorted(mixture.heights, h, side="right") - 1,
            len(mixture.heights) - 2,
        )
        t = (h - mixture.heights[row]) / (
            mixture.heights[row + 1] - mixture.heights[row]
        )
        best, selected = np.full(len(h), -np.inf), np.full(len(h), -1, dtype=np.int16)
        matrix = (
            np.full((len(h), len(mixture.taxa)), -np.inf)
            if score_mode == "matrix"
            else None
        )
        for taxon, field, seed in self.entries:
            if taxon not in mixture.taxa:
                continue
            i = mixture.taxa.index(taxon)
            cumulative = mixture.cumulative[:, i]
            weights = cumulative - (mixture.cumulative[:, i - 1] if i else 0)
            w = (1 - t) * weights[row] + t * weights[row + 1]
            active = w > 0
            if not active.any():
                continue
            local = species_site_uniforms(keys[active], seed)
            u = field.uniforms(p[active], local, strength)
            score = gumbel_scores(w[active], u)
            if matrix is not None:
                matrix[active, i] = score
            index = np.flatnonzero(active)
            win = (score > best[index]) | (
                (score == best[index]) & (i < selected[index])
            )
            selected[index[win]], best[index[win]] = i, score[win]
        if matrix is not None:
            reference = np.where(
                np.isfinite(matrix).any(axis=1), np.argmax(matrix, axis=1), -1
            )
            np.testing.assert_array_equal(selected, reference)
        return selected
