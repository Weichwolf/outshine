"""Run one test in an owned session and reap it before returning its verdict."""

import ctypes
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

PR_SET_CHILD_SUBREAPER = 36


def reap_descendants(group):
    while True:
        try:
            child, _ = os.waitpid(-group, os.WNOHANG)
        except ChildProcessError:
            return
        if child == 0:
            return


def signal_group(group, number):
    try:
        os.killpg(group, number)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        if number == 0:
            return True
        raise


def clean_group(process, grace):
    group = process.pid
    signal_group(group, signal.SIGTERM)
    deadline = time.monotonic() + grace
    while process.poll() is None and time.monotonic() < deadline:
        time.sleep(0.02)
    signal_group(group, signal.SIGKILL)
    deadline = time.monotonic() + 5
    process.wait(timeout=max(0, deadline - time.monotonic()))
    while True:
        reap_descendants(group)
        if not signal_group(group, 0):
            return
        if time.monotonic() >= deadline:
            raise TimeoutError(f'owned process group {group} survived cleanup')
        time.sleep(0.02)


def enable_child_reaping():
    if sys.platform.startswith('linux'):
        # Linux otherwise hands orphaned grandchildren to an unrelated subreaper or PID 1.
        libc = ctypes.CDLL(None, use_errno=True)
        libc.prctl.argtypes = (ctypes.c_int, ctypes.c_ulong, ctypes.c_ulong,
                              ctypes.c_ulong, ctypes.c_ulong)
        if libc.prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) != 0:
            error = ctypes.get_errno()
            raise OSError(error, os.strerror(error))


def supervise(owner, seconds, log, marker, command):
    stopped = 0

    def stop(number, _frame):
        nonlocal stopped
        stopped = number

    for number in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
        signal.signal(number, stop)
    enable_child_reaping()
    ready = Path(str(marker) + '.start')
    while not ready.exists():
        if stopped or os.getppid() != owner:
            return 128 + (stopped or signal.SIGTERM)
        time.sleep(0.01)
    ready.unlink()
    if stopped or os.getppid() != owner:
        return 128 + (stopped or signal.SIGTERM)
    with log.open('wb') as output:
        process = subprocess.Popen(command, stdout=output, stderr=output, start_new_session=True)
    timed_out = False
    try:
        deadline = time.monotonic() + seconds
        while process.poll() is None:
            if stopped or os.getppid() != owner:
                stopped = stopped or signal.SIGTERM
                break
            if time.monotonic() >= deadline:
                marker.touch()
                timed_out = True
                break
            time.sleep(0.02)
    finally:
        clean_group(process, 2 if stopped or timed_out else 0)
    if stopped:
        return 128 + stopped
    return process.returncode if process.returncode >= 0 else 128 - process.returncode


def main():
    owner, seconds, log, marker, *command = sys.argv[1:]
    marker = Path(marker)
    try:
        return supervise(int(owner), float(seconds), Path(log), marker, command)
    except (OSError, TimeoutError, subprocess.TimeoutExpired) as error:
        Path(str(marker) + '.cleanup-failed').touch()
        print(f'run_with_timeout: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
