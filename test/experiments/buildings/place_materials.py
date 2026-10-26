"""Deduplicate tile-local procedural material bindings when composing one Place scene."""

import json

import numpy as np
from outshine_ref.buildings.facade_finish import FacadeFinish


def default_finish():
    finish = FacadeFinish()
    return {"frame_m": finish.frame_m, "band_m": finish.band_m, "band_tint": 0.65}


class Palette:
    def __init__(self):
        self.ids = {}
        self.definitions = {}
        self.facades = {}

    def identifier(self, kind, value):
        key = kind, json.dumps(value, sort_keys=True)
        if key not in self.ids:
            index = 12 + len(self.ids)
            if index >= 65536:
                raise ValueError("Place procedural material capacity exceeded")
            self.ids[key] = index
            (self.definitions if kind == "pbr" else self.facades)[str(index)] = value
        return self.ids[key]

    def bind(self, mesh, definitions, facades):
        remap = np.arange(65536, dtype=np.uint16)
        for old, values in definitions.items():
            remap[int(old)] = self.identifier("pbr", values)
        for old, values in facades.items():
            binding = values | {"wall_material": int(remap[values["wall_material"]])}
            remap[int(old)] = self.identifier("facade", binding)
        return mesh | {"materials": remap[mesh["materials"]]}
