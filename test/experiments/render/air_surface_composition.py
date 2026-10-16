"""Independent affine layer reference for #266, in linear scene RGB units.

Events are ordered from eye to background. Surface radiance excludes transmitted
background, is coverage-premultiplied, and uses the same units as air radiance.
Surface RGB transmittance includes uncovered area. Prefixes contain all media
before the surface, without surface attenuation. Clear air alone is insufficient
when foreground clouds or fog contribute. No refraction, angular mixing or motion
model; constructing a converged joint medium is a separate requirement.
"""

import argparse
import json
from pathlib import Path

import numpy as np


def compose_events(background_eye, air_radiance, air_transmittance,
                   surface_radiance, surface_transmittance):
    """Compose sorted surfaces without dividing by possibly zero air throughput."""
    result = np.zeros(3)
    through = np.ones(3)
    for light, air_t, surface, surface_t in zip(
            air_radiance, air_transmittance, surface_radiance,
            surface_transmittance, strict=True):
        result += through * (air_t * surface + (1 - surface_t) * light)
        through *= surface_t
    return result + through * background_eye


def nested_transfer(background, air_source, air_t, surface, surface_t):
    """Independent physical order: walk local air/surface layers from the back."""
    colour = air_source[-1] + air_t[-1] * background
    for i in reversed(range(len(surface))):
        colour = surface[i] + surface_t[i] * colour
        colour = air_source[i] + air_t[i] * colour
    return colour


def prefixes(air_source, air_t):
    light, through = np.zeros(3), np.ones(3)
    result_l, result_t = [], []
    for source, transmittance in zip(air_source, air_t, strict=True):
        light = light + through * source
        through = through * transmittance
        result_l.append(light.copy())
        result_t.append(through.copy())
    return np.array(result_l), np.array(result_t)


def check_layers(background, air_source, air_t, surface, surface_t):
    light, through = prefixes(air_source, air_t)
    background_eye = light[-1] + through[-1] * background
    actual = compose_events(background_eye, light[:-1], through[:-1],
                            surface, surface_t)
    expected = nested_transfer(background, air_source, air_t, surface, surface_t)
    error = float(np.max(np.abs(actual - expected)))
    np.testing.assert_allclose(actual, expected, rtol=2e-13, atol=2e-13)
    return error


def run_checks():
    rng = np.random.default_rng(266)
    worst, cases = 0.0, 0
    for count in (0, 1, 2, 8, 32):
        for _ in range(128):
            air_t = np.exp(-rng.uniform(0, 20, (count + 1, 3)))
            air_source = rng.uniform(0, 3, (count + 1, 3)) * (1 - air_t)
            surface_t = rng.uniform(0, 1, (count, 3))
            surface = rng.uniform(0, 4, (count, 3)) * (1 - surface_t)
            background = rng.uniform(0, 10, 3)
            worst = max(worst, check_layers(background, air_source, air_t,
                                            surface, surface_t))
            cases += 1
            # Insert a neutral event at every depth: RGB identity is mandatory.
            light, through = prefixes(air_source, air_t)
            eye = light[-1] + through[-1] * background
            neutral = compose_events(eye, light[:-1], through[:-1],
                                     np.zeros((count, 3)), np.ones((count, 3)))
            np.testing.assert_array_equal(neutral, eye)
            cases += 1
    for air_t in (np.ones((2, 3)), np.zeros((2, 3)),
                  np.array([[0, 1, 0.3], [1, 0.1, 0]])):
        # Includes vacuum, opaque air, opaque surfaces and coloured absorption.
        for surface_t in (np.zeros((1, 3)), np.ones((1, 3)),
                          np.array([[0, 0.5, 1]])):
            source = np.full((2, 3), 0.2) * (1 - air_t)
            surface = np.array([[0.1, 0.3, 0.8]]) * (1 - surface_t)
            worst = max(worst, check_layers(np.array([0.8, 0.4, 0.1]),
                                            source, air_t, surface, surface_t))
            cases += 1
    # Ordinary coverage blending uses premultiplied source and scalar 1-alpha.
    for alpha in (0, 0.01, 0.5, 1):
        air_t = np.array([[0.8, 0.7, 0.9], [0.2, 0.4, 0.3]])
        source = np.array([[0.1, 0.2, 0.3], [0.3, 0.2, 0.1]]) * (1 - air_t)
        worst = max(worst, check_layers(np.array([1, 2, 3]), source, air_t,
                                        np.array([[2, 1, 3]]) * alpha,
                                        np.full((1, 3), 1 - alpha)))
        cases += 1
        # Coverage is a mixture of an uncovered ray and the physical surface.
        for physical_t in (np.zeros((1, 3)), np.ones((1, 3)),
                           np.array([[0.1, 0.5, 0.9]])):
            background = np.array([1, 2, 3])
            intrinsic_surface = np.array([[2, 1, 3]])
            uncovered = nested_transfer(background, source, air_t,
                                         np.zeros((1, 3)), np.ones((1, 3)))
            covered = nested_transfer(background, source, air_t,
                                       intrinsic_surface, physical_t)
            light, through = prefixes(source, air_t)
            actual = compose_events(uncovered, light[:-1], through[:-1],
                                     alpha * intrinsic_surface,
                                     (1 - alpha) + alpha * physical_t)
            expected = (1 - alpha) * uncovered + alpha * covered
            np.testing.assert_allclose(actual, expected, rtol=2e-13, atol=2e-13)
            worst = max(worst, float(np.max(np.abs(actual - expected))))
            cases += 1
    front_t = np.exp(-0.1)
    front_l = 0.2 * (1 - front_t)
    eye = front_l + front_t * 0.8
    collapsed_then_fogged = front_l + front_t * eye
    assert abs(collapsed_then_fogged - eye) / eye > 0.06
    postfog_example = {'background_eye': eye,
                       'incorrect_result': collapsed_then_fogged,
                       'relative_identity_error': abs(collapsed_then_fogged - eye) / eye}
    # A coloured surface must not tint light emitted by foreground clouds.
    sigma_air = np.array([0.1, 0.2, 0.3])
    sigma_cloud = np.array([0.8, 0.7, 0.6])
    j_air = np.array([0.02, 0.03, 0.04])
    j_cloud = np.array([0.2, 0.3, 0.4])
    joint_sigma = sigma_air + sigma_cloud
    joint_t = np.exp(-joint_sigma)
    joint_l = (j_air + j_cloud) * -np.expm1(-joint_sigma) / joint_sigma
    clear_t = np.exp(-sigma_air)
    clear_l = j_air * -np.expm1(-sigma_air) / sigma_air
    intrinsic = np.array([0.8, 0.3, 0.1])
    eye = joint_l + joint_t * intrinsic
    surface = np.zeros(3)
    surface_t = np.array([0.4, 0.8, 0.1])
    correct = compose_events(eye, [joint_l], [joint_t], [surface], [surface_t])
    local = joint_l + joint_t * (surface + surface_t * intrinsic)
    np.testing.assert_allclose(correct, local, rtol=2e-13, atol=2e-13)
    clear_only = compose_events(eye, [clear_l], [clear_t], [surface], [surface_t])
    clear_front_error = float(np.max(np.abs(clear_only - local)))
    assert clear_front_error > 0.1
    cases += 1
    return {'cases': cases, 'max_absolute_rgb_error': worst,
            'neutral_identity': 'exact', 'division_by_air_transmittance': False,
            'old_postpass_counterexample': postfog_example,
            'clear_only_front_with_cloud_max_absolute_error': clear_front_error,
            'scope': 'Affine RGB layer algebra; no raster/depth/velocity/Place proof'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    result = run_checks()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
