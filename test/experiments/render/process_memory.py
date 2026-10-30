"""Peak RSS of the current executable image on the two research hosts."""

import resource
import sys
from pathlib import Path


def rss_bytes(value, platform):
    if platform == "darwin":
        return int(value)
    if platform.startswith("linux"):
        return int(value) * 1024
    raise ValueError("ru_maxrss unit is not qualified for platform " + platform)


def peak_rss_bytes():
    if sys.platform.startswith("linux"):
        # Linux rusage retains a previous executable's peak across exec. VmHWM
        # belongs to the current address space and excludes that inherited peak.
        return linux_peak_rss_bytes(Path("/proc/self/status").read_text())
    return rss_bytes(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss, sys.platform)


def linux_peak_rss_bytes(status):
    for line in status.splitlines():
        if line.startswith("VmHWM:"):
            fields = line.split()
            if len(fields) == 3 and fields[2] == "kB" and fields[1].isdigit():
                return int(fields[1]) * 1024
            break
    raise ValueError("Linux current-image VmHWM is unavailable")
