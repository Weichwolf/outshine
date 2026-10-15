"""Bound CPU reference wavefront memory without reducing total samples or image size."""


def integrator(config, spp):
    limit = config.get("samples_per_pass")
    options = {}
    if limit is not None:
        if (
            isinstance(limit, bool)
            or not isinstance(limit, int)
            or limit < 1
            or limit > spp
            or spp % limit
        ):
            raise ValueError("Samples per pass must be a positive divisor of total spp")
        # Mitsuba's automatic choice exceeded this host's RAM for Tokyo. Its
        # supported explicit limit bounds the reference despite deprecation.
        options["samples_per_pass"] = limit
    path = dict(type="path", max_depth=12, rr_depth=5, **options)
    if config.get("denoise"):
        return dict(
            type="aov", aovs="albedo:albedo,normal:sh_normal", image=path, **options
        )
    return path
