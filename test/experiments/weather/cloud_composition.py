"""Check first-cloud-extinction composition against exact independent slab transfer."""

import json

import numpy as np


def exact_transfer(slabs, background):
    result = np.array(background, dtype=float)
    for air_tau, cloud_tau, air_source, cloud_source in reversed(slabs):
        total = air_tau + cloud_tau
        if total == 0.0:
            continue
        source = (air_tau * air_source + cloud_tau * cloud_source) / total
        through = np.exp(-total)
        result = through * result + (1.0 - through) * source
    return result


def cloud_events(slabs, background):
    # No in-scattering event is preintegrated twice: sources are j / sigma_t.
    clear_background = exact_transfer([(a, 0.0, sa, sc) for a, _, sa, sc in slabs], background)
    cloud_through = 1.0
    air_through = 1.0
    air_front = np.zeros(3)
    events = np.zeros(3)
    for air_tau, cloud_tau, air_source, cloud_source in slabs:
        assert air_tau == 0.0 or cloud_tau == 0.0
        if air_tau:
            step_through = np.exp(-air_tau)
            air_front += air_through * (1.0 - step_through) * air_source
            air_through *= step_through
        if cloud_tau:
            step_through = np.exp(-cloud_tau)
            events += cloud_through * (1.0 - step_through) * (air_front + air_through * cloud_source)
            cloud_through *= step_through
    return cloud_through * clear_background + events


def controls():
    generator = np.random.default_rng(193)
    errors = []
    for count in range(1, 33):
        for _ in range(16):
            slabs = []
            for _ in range(count):
                tau = generator.uniform(0.0, 8.0)
                air_source, cloud_source = generator.uniform(0.0, 4.0, (2, 3))
                is_cloud = generator.choice([True, False])
                slabs.append((0.0 if is_cloud else tau, tau if is_cloud else 0.0,
                              air_source, cloud_source))
            background = generator.uniform(0.0, 4.0, 3)
            error = np.max(np.abs(exact_transfer(slabs, background) - cloud_events(slabs, background)))
            errors.append(float(error))
    assert max(errors) < 4e-15
    air_source = np.array([0.2, 0.5, 0.9])
    black = np.zeros(3)
    before_cloud = [(2.0, 0.0, air_source, black), (0.0, 20.0, black, black)]
    measured = cloud_events(before_cloud, black)
    np.testing.assert_allclose(measured, (1.0 - np.exp(-2.0)) * air_source, atol=1e-15)
    naive = np.exp(-20.0) * exact_transfer([(2.0, 0.0, air_source, black)], black)
    assert np.max(np.abs(naive - measured)) > 0.7
    return dict(slab_cases=len(errors), max_abs_error=max(errors),
                scope='Exact separated slabs; mixed-medium quadrature and engine images remain separate.')


if __name__ == '__main__':
    print(json.dumps(controls()))
