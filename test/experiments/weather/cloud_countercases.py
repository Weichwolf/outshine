"""Stratified elevation, inside/above and thin-cloud counterexamples for bounded sampling."""

import argparse
from dataclasses import replace
import json
from pathlib import Path

import numpy as np

from cloud_field import Layer, optical_depth
from cloud_composition import controls as composition_controls
from cloud_controls import controls


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    controls()
    generator = np.random.default_rng(193)
    elevation = np.linspace(-0.999, 0.999, 512)
    azimuth = generator.uniform(0.0, 2.0 * np.pi, len(elevation))
    horizontal = np.sqrt(1.0 - elevation * elevation)
    directions = np.stack([np.cos(azimuth) * horizontal, elevation, np.sin(azimuth) * horizontal], axis=-1)
    records = []
    for name, height, cover, extinction in [('below-overcast', 1772.0, 1.0, 0.012),
                                           ('below-broken', 1772.0, 0.65, 0.012),
                                           ('inside-broken', 2800.0, 0.65, 0.012),
                                           ('above-broken', 4000.0, 0.65, 0.012),
                                           ('below-thin', 1772.0, 1.0, 0.001)]:
        layer = replace(Layer(), cover=cover, extinction_per_m=extinction)
        origin = np.array([0.0, height, 0.0])
        reference_tau = optical_depth(origin, directions, layer, 512)
        finer = np.exp(-optical_depth(origin, directions, layer, 1024))
        reference = np.exp(-reference_tau)
        convergence = float(np.max(np.abs(finer - reference)))
        assert convergence < 0.001
        for samples in [1, 8, 16, 32, 64]:
            measured_tau = optical_depth(origin, directions, layer, samples)
            error = np.abs(np.exp(-measured_tau) - reference)
            record = dict(case=name, samples=samples,
                          p99_abs=float(np.quantile(error, 0.99)), max_abs=float(error.max()),
                          tau_rmse=float(np.sqrt(np.mean((measured_tau-reference_tau)**2))),
                          reference_512_to_1024_max=convergence)
            records.append(record)
            print(json.dumps(record))
    report = dict(cases=records, composition=composition_controls(),
                  scope='Stratified declared fields, not observed weather or a complete runtime/GPU gate.')
    (args.output / 'countercases.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
