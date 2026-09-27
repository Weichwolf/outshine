"""Prove relation checks reject changed pixels, channels and declarations."""

import copy
import unittest

import numpy as np

from corpus_invariants import evaluate


class Invariants(unittest.TestCase):
    def setUp(self):
        self.frame = np.zeros((8, 12, 4), dtype=np.float32)
        self.frame[1:5, 1:5, :3] = [0.25, 0.5, 0.75]
        self.frame[1:5, 7:11, :3] = [0.25, 0.5, 0.75]
        self.frame[..., 3] = 1
        self.manifest = {
            "renders": {"default": {"resolutionX": 12, "resolutionY": 8}},
            "statedInvariants": [{
                "kind": "region-compare", "name": "translated",
                "fromPx": {"value": [1, 1, 4, 4]}, "toPx": {"value": [7, 1, 4, 4]},
                "channels": [0, 1, 2], "scale": {"value": 1},
                "maxUlps": {"value": 0},
            }],
        }

    def check(self):
        return evaluate(self.frame, self.manifest)[0][1]

    def test_exact_relation_and_one_component_mutation(self):
        self.assertTrue(self.check())
        self.frame[3, 3, 1] = np.nextafter(np.float32(.5), np.float32(1))
        self.assertFalse(self.check())
        self.manifest["statedInvariants"][0]["maxUlps"]["value"] = 1
        self.assertTrue(self.check())

    def test_wrong_channel_scale_rectangle_and_extent(self):
        self.frame[3, 3, 0] = 0
        self.manifest["statedInvariants"][0]["channels"] = [1, 2]
        self.assertTrue(self.check())
        self.manifest["statedInvariants"][0]["channels"] = [0, 1, 2]
        self.assertFalse(self.check())
        self.frame[3, 3, 0] = .25
        self.manifest["statedInvariants"][0]["scale"]["value"] = .5
        self.assertFalse(self.check())
        self.manifest["statedInvariants"][0]["scale"]["value"] = 1
        self.manifest["statedInvariants"][0]["fromPx"]["value"] = [0, 1, 4, 4]
        self.assertFalse(self.check())
        self.manifest["statedInvariants"][0]["fromPx"]["value"] = [11, 1, 4, 4]
        with self.assertRaises(ValueError):
            self.check()
        self.manifest["renders"]["default"]["resolutionX"] = 13
        with self.assertRaises(ValueError):
            self.check()

    def test_empty_nonfinite_and_missing_currency_refuse(self):
        self.frame.fill(0)
        with self.assertRaises(ValueError):
            self.check()
        self.frame[2, 2, 0] = np.nan
        with self.assertRaises(ValueError):
            self.check()
        self.frame[2, 2, 0] = .25
        del self.manifest["statedInvariants"][0]["maxUlps"]
        with self.assertRaises(ValueError):
            self.check()

    def test_relative_quantile_and_hue(self):
        relation = self.manifest["statedInvariants"][0]
        del relation["maxUlps"]
        relation["maxP95Relative"] = {"value": .1}
        self.frame[3, 3, 0] = .4
        self.assertTrue(self.check())
        self.frame[1:5, 1:5, 0] = .4
        self.assertFalse(self.check())
        self.frame[1:5, 1:5, :3] = [.25, .5, .75]
        hue = copy.deepcopy(self.manifest)
        hue["statedInvariants"] = [{
            "kind": "hue-of-brightest", "name": "highlight",
            "brightestFraction": {"value": .1}, "hue": {"value": [1 / 6, 1 / 3, .5]},
            "maxHueError": {"value": 1e-6},
        }]
        self.assertTrue(evaluate(self.frame, hue)[0][1])
        hue["statedInvariants"][0]["hue"]["value"] = [.5, 1 / 3, 1 / 6]
        self.assertFalse(evaluate(self.frame, hue)[0][1])


if __name__ == "__main__":
    unittest.main()
