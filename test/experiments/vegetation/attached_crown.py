"""Complete compact attached shoots over a retained camera-independent source."""

import hashlib
import json

import numpy as np
from attached_crown_build import compile_twigs
from attached_crown_query import evaluate
from compact_crown import CompactCrown
from outshine_ref.vegetation.plan_arrays import immutable_array


class AttachedCrown:
    def __init__(self, base, data, recipe):
        if base.data["origins"].dtype != np.float64:
            raise ValueError("Float64 source required")
        count = len(data["ids"])
        if (
            count > 16384
            or data["counts"].shape != (count,)
            or data["host_offsets"].shape != (len(base.data["hosts"]) + 1,)
            or int(data["host_offsets"][-1]) != count
            or data["leaf_offsets"].shape != (count + 1,)
            or int(data["leaf_offsets"][-1]) != len(base.data["variants"])
            or any(not np.isfinite(v).all() for v in data.values())
        ):
            raise ValueError("bounded complete finite attached-twig data required")
        self.base = base
        self.data = {k: immutable_array(v) for k, v in data.items()}
        self.lower, self.upper = self.data["lower"], self.data["upper"]
        self.recipe_json = json.dumps(recipe, sort_keys=True)
        digest = hashlib.sha256((base.identity + self.recipe_json).encode())
        for prefix, arrays in (("base", base.data), ("twig", self.data)):
            for key, value in sorted(arrays.items()):
                digest.update(
                    (prefix + key + str((value.dtype.str, value.shape))).encode()
                )
                digest.update(value.tobytes())
        self.identity = digest.hexdigest()

    @classmethod
    def build(cls, source, *, leaf_limit=6, max_twigs=16384):
        if (
            type(leaf_limit) is not int
            or not 4 <= leaf_limit <= 16
            or type(max_twigs) is not int
            or not 0 <= max_twigs <= 16384
        ):
            raise ValueError("bounded leaf partition and twig capacity required")
        needed = int(((source.crown.counts + leaf_limit - 1) // leaf_limit).sum())
        if needed > max_twigs:
            return None
        base = CompactCrown.build(source)
        data, recipe = compile_twigs(source, base, leaf_limit)
        return cls(base, data, recipe)

    def evaluate(self, selected, *, max_leaves, anchors=False):
        return evaluate(
            self.base, self.data, selected, max_leaves=max_leaves, anchors=anchors
        )

    def save(self, path):
        arrays = {"base__" + k: v for k, v in self.base.data.items()}
        arrays.update({"twig__" + k: v for k, v in self.data.items()})
        np.savez(
            path,
            **arrays,
            recipe=np.asarray(self.recipe_json),
            source=np.asarray(self.base.identity),
            identity=np.asarray(self.identity),
        )

    @classmethod
    def load(cls, path, *, expected_source):
        with np.load(path, allow_pickle=False) as z:
            if str(z["source"]) != expected_source:
                raise ValueError("unexpected source identity")
            base = CompactCrown(
                expected_source,
                {k[6:]: z[k] for k in z.files if k.startswith("base__")},
            )
            result = cls(
                base,
                {k[6:]: z[k] for k in z.files if k.startswith("twig__")},
                json.loads(str(z["recipe"])),
            )
            if str(z["identity"]) != result.identity:
                raise ValueError("attached product content identity mismatch")
        return result
