"""Units for the two hosts used by the focused Python rendering experiments."""

import resource
import sys


def rss_bytes(value, platform):
    if platform == "darwin":
        return int(value)
    if platform.startswith("linux"):
        return int(value) * 1024
    raise ValueError("ru_maxrss unit is not qualified for platform " + platform)


def peak_rss_bytes():
    return rss_bytes(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss, sys.platform)
