"""Authored shoot length and lamina roll on the unchanged saved crown inventory."""

import hashlib
import json

import numpy as np
from compact_crown import CompactCrown
from outshine_ref.vegetation.plan_arrays import immutable_array


def form_bounds(base, scale, reach):
    origin = base.data["origins"]
    low = origin + scale[:, None] * (base.data["lower"] + reach - origin) - reach
    high = origin + scale[:, None] * (base.data["upper"] - reach - origin) + reach
    guard = 512 * np.finfo(float).eps * (np.max(abs(origin), axis=1) + reach + 3)
    return (
        np.nextafter(low - guard[:, None], -np.inf),
        np.nextafter(high + guard[:, None], np.inf),
    )


def rolled_frames(frames, angle):
    c, s = np.cos(angle)[:, None], np.sin(angle)[:, None]
    result = np.empty_like(frames)
    result[:, :, 0] = c * frames[:, :, 0] - s * frames[:, :, 2]
    result[:, :, 1] = frames[:, :, 1]
    result[:, :, 2] = s * frames[:, :, 0] + c * frames[:, :, 2]
    return result


class ShootForm:
    def __init__(self, base, scale, roll, lower, upper, recipe):
        if base.data["origins"].dtype != np.dtype(np.float64):
            raise ValueError(
                "original Float64 product required; Float32 is not admitted"
            )
        scale, roll = np.asarray(scale, float), np.asarray(roll, float)
        low, high = np.asarray(lower, float), np.asarray(upper, float)
        count = len(base.data["hosts"])
        if (
            scale.shape != (count,)
            or roll.shape != base.data["variants"].shape
            or low.shape != (count, 3)
            or high.shape != low.shape
            or not all(np.isfinite(v).all() for v in (scale, roll, low, high))
            or np.any((scale < 0.2) | (scale > 1.8))
            or np.any(abs(roll) > np.pi / 2)
            or np.any(low > high)
        ):
            raise ValueError(
                "bounded complete source-host scales/leaf rolls/bounds required"
            )
        self.base = base
        self.scale, self.roll = immutable_array(scale), immutable_array(roll)
        self.lower, self.upper = immutable_array(low), immutable_array(high)
        self.recipe_json = json.dumps(recipe, sort_keys=True)
        digest = hashlib.sha256((base.identity + self.recipe_json).encode())
        for key, value in sorted(base.data.items()):
            digest.update(key.encode())
            digest.update(str((value.dtype.str, value.shape)).encode())
            digest.update(value.tobytes())
        for value in (self.scale, self.roll, self.lower, self.upper):
            digest.update(value.tobytes())
        self.identity = digest.hexdigest()

    @classmethod
    def build(
        cls,
        source,
        base,
        *,
        seed=727,
        scale_spread=0.0,
        leaf_roll_spread=0.0,
        host_roll_spread=0.0,
    ):
        if (
            base.identity != source.identity
            or base.data["origins"].dtype != np.dtype(np.float64)
            or type(seed) is not int
            or not 0 <= seed < 2**64
            or not np.isfinite((scale_spread, leaf_roll_spread, host_roll_spread)).all()
            or not 0 <= scale_spread <= 0.75
            or min(leaf_roll_spread, host_roll_spread) < 0
            or leaf_roll_spread + host_roll_spread > np.pi / 2
        ):
            raise ValueError(
                "original source and bounded explicit authored traits required"
            )
        crown = source.crown
        rng = np.random.Generator(np.random.PCG64(seed))
        latent = rng.uniform(-1, 1, len(crown.parents))
        variation = (
            0.65 * latent[crown.parents[crown.hosts]] + 0.35 * latent[crown.hosts]
        )
        variation -= variation.mean()
        maximum = float(abs(variation).max(initial=0))
        if maximum:
            variation /= maximum
        scale = 1 + scale_spread * variation
        roll = leaf_roll_spread * rng.uniform(-1, 1, len(crown.variants))
        roll += host_roll_spread * np.repeat(latent[crown.hosts], crown.counts)
        reach = crown.petiole_m + max(
            b.length + b.petiole + b.width / 2 * (4 / 3) * np.hypot(1, b.fold)
            for b in source.blades
        )
        low, high = form_bounds(base, scale, reach)
        recipe = {
            "format": "authored-shoot-form-1",
            "numpy": np.__version__,
            "seed": seed,
            "scale_spread": scale_spread,
            "leaf_roll_spread": leaf_roll_spread,
            "host_roll_spread": host_roll_spread,
            "source": source.identity,
        }
        return cls(base, scale, roll, low, high, recipe)

    def evaluate(self, selected, *, max_leaves, anchors=False):
        poses = self.base.evaluate(selected, max_leaves=max_leaves, anchors=True)
        if poses is None:
            return None
        owner = np.repeat(
            np.flatnonzero(selected),
            np.diff(self.base.data["offsets"])[selected].astype(int),
        )
        scale = self.scale[owner]
        if np.any(scale != 1):
            delta = (scale - 1)[:, None] * (
                poses["nodes"] - self.base.data["origins"][owner]
            )
            poses["origins"] = poses["origins"] + delta
            poses["nodes"] = poses["nodes"] + delta
        angle = self.roll[poses["ids"]]
        if np.any(angle != 0):
            poses["frames"] = rolled_frames(poses["frames"], angle)
        if not anchors:
            del poses["nodes"]
        for value in poses.values():
            value.setflags(write=False)
        return poses

    def save(self, path):
        np.savez(
            path,
            metadata=np.asarray(self.recipe_json),
            identity=np.asarray(self.identity),
            scale=self.scale,
            roll=self.roll,
            lower=self.lower,
            upper=self.upper,
            **{f"base_{key}": value for key, value in self.base.data.items()},
        )

    @classmethod
    def load(cls, path, *, expected_source):
        with np.load(path, allow_pickle=False) as z:
            recipe = json.loads(str(z["metadata"]))
            if (
                recipe.get("source") != expected_source
                or recipe.get("format") != "authored-shoot-form-1"
            ):
                raise ValueError(
                    "same declared source and authored form format required"
                )
            base = CompactCrown(
                expected_source, {k[5:]: z[k] for k in z.files if k.startswith("base_")}
            )
            result = cls(base, z["scale"], z["roll"], z["lower"], z["upper"], recipe)
            if result.identity != str(z["identity"]):
                raise ValueError("saved form coefficient identity mismatch")
        return result
