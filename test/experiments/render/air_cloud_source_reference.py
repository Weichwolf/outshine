"""Independent direct air-source shadow controls in a stratified plane slab (#320).

A prescribed diffuse/Psi input stays fixed to isolate the direct-source error.
This does not solve cloud multiple scattering, spherical LUTs or runtime images.
"""

import argparse
import json
from dataclasses import dataclass
from itertools import pairwise
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]


@dataclass(frozen=True)
class Slab:
    eye_height: float = 0.2
    view_mu: float = 0.4
    sun_mu: float = 0.8
    air_top: float = 10.0
    cloud_low: float = 1.0
    cloud_high: float = 2.0
    cloud_sigma: float = 1.5

    def reach(self, query):
        if self.view_mu == 0:
            return max(0, query)
        boundary = self.air_top if self.view_mu > 0 else 0
        return max(0, min(query, (boundary - self.eye_height) / self.view_mu))

    def height(self, distance):
        return self.eye_height + self.view_mu * np.asarray(distance)

    def eye_cloud_column(self, distance):
        distance = np.asarray(distance)
        if self.view_mu == 0:
            return distance * (self.cloud_low < self.eye_height < self.cloud_high)
        crossings = np.sort(
            (np.array([self.cloud_low, self.cloud_high]) - self.eye_height)
            / self.view_mu
        )
        return np.maximum(0, np.minimum(distance, crossings[1]) - max(0, crossings[0]))

    def sun_cloud_column(self, distance):
        if self.sun_mu <= 0:
            return np.zeros_like(np.asarray(distance))
        height = np.clip(self.height(distance), self.cloud_low, self.cloud_high)
        return (self.cloud_high - height) / self.sun_mu

    def cuts(self, query):
        end = self.reach(query)
        cuts = [0, end]
        if self.view_mu != 0:
            cuts.extend(
                np.clip(
                    (np.array([self.cloud_low, self.cloud_high]) - self.eye_height)
                    / self.view_mu,
                    0,
                    end,
                )
            )
        return np.unique(cuts)


SIGMA = np.array([0.02, 0.04, 0.08])
DIRECT = np.array([0.01, 0.02, 0.04]) / (4 * np.pi)
DIFFUSE = np.array([0.0001, 0.0002, 0.0003])
MODES = ("correct", "unshadowed_source", "dim_whole_source", "dim_eye_radiance")


def log_weight(slab, distance, direct, shadow):
    distance = np.asarray(distance)
    value = -distance[..., None] * SIGMA
    value -= slab.cloud_sigma * slab.eye_cloud_column(distance)[..., None]
    if direct:
        if slab.sun_mu <= 0:
            return np.full_like(value, -np.inf)
        value -= (slab.air_top - slab.height(distance))[..., None] * SIGMA / slab.sun_mu
    if shadow:
        value -= slab.cloud_sigma * slab.sun_cloud_column(distance)[..., None]
    return value


def exponential_segment(first, last, width):
    """Exact positive integral of an affine log weight, including zero slope."""
    difference = np.abs(last - first)
    ratio = np.divide(
        -np.expm1(-difference),
        difference,
        out=np.ones_like(first),
        where=difference > 0,
    )
    return np.exp(np.maximum(first, last)) * width * ratio


def analytic(slab, query, mode="correct"):
    if mode == "dim_eye_radiance":
        result = analytic(slab, query, "unshadowed_source")
        return result * np.exp(-slab.cloud_sigma * slab.sun_cloud_column(0))
    cuts = slab.cuts(query)
    result = np.zeros(3)
    for first, last in pairwise(cuts):
        for direct, coefficient in ((True, DIRECT), (False, DIFFUSE)):
            if direct and slab.sun_mu <= 0:
                continue
            shadow = (
                direct and mode != "unshadowed_source"
            ) or mode == "dim_whole_source"
            a = log_weight(slab, first, direct, shadow)
            b = log_weight(slab, last, direct, shadow)
            result += coefficient * exponential_segment(a, b, last - first)
    return result


def quadrature(slab, query, nodes=64):
    x, weights = np.polynomial.legendre.leggauss(nodes)
    cuts = slab.cuts(query)
    result = np.zeros(3)
    for first, last in pairwise(cuts):
        half = (last - first) / 2
        distance = first + (x + 1) * half
        eye = np.exp(
            -distance[:, None] * SIGMA
            - slab.cloud_sigma * slab.eye_cloud_column(distance)[:, None]
        )
        solar = np.zeros((nodes, 3))
        if slab.sun_mu > 0:
            solar = np.exp(
                -(slab.air_top - slab.height(distance))[:, None] * SIGMA / slab.sun_mu
                - slab.cloud_sigma * slab.sun_cloud_column(distance)[:, None]
            )
        result += half * np.einsum("i,ic->c", weights, eye * (DIRECT * solar + DIFFUSE))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private output belongs outside the checkout")
    scenarios = (
        Slab(),
        Slab(eye_height=1.5, view_mu=0.6, sun_mu=0.4),
        Slab(eye_height=3, view_mu=-0.3, sun_mu=0.8),
        Slab(view_mu=0, sun_mu=0.4),
        Slab(cloud_sigma=0),
        Slab(view_mu=0.8, sun_mu=0.8),
    )
    report = {"scope": __doc__, "cases": []}
    for scenario in scenarios:
        distance = scenario.reach(12)
        correct = analytic(scenario, distance)
        numerical = quadrature(scenario, distance)
        refined = quadrature(scenario, distance, 128)
        error = float(np.max(np.abs(correct - numerical)))
        if error > 1e-12 or np.max(np.abs(numerical - refined)) > 1e-12:
            raise ValueError("independent optical/source integration did not converge")
        reference = np.maximum(1e-5, correct)
        wrong = {
            mode: float(
                np.max(np.abs(analytic(scenario, distance, mode) - correct) / reference)
            )
            for mode in MODES[1:]
        }
        report["cases"].append(
            {
                "parameters": scenario.__dict__,
                "query_km": distance,
                "correct_per_lux_rgb": correct.tolist(),
                "refinement_abs": error,
                "wrong_relative_max": wrong,
            }
        )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
