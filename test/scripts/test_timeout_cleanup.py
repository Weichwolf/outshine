import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(os.environ.get('OUTSHINE_RUN_SOURCE', ROOT / 'test/run.sh'))


def alive(pid):
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False


class TimeoutCleanup(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='outshine-timeout-cleanup-')
        self.directory = Path(self.temp.name)
        self.runners = []
        self.children = []
        self.child = self.directory / 'ignores-term.py'
        self.child.write_text('import os,signal,sys,time\n'
                              'from pathlib import Path\n'
                              'signal.signal(signal.SIGTERM,signal.SIG_IGN)\n'
                              'Path(sys.argv[1]).write_text(str(os.getpid()))\n'
                              'while True: time.sleep(1)\n')
        self.foreign_pid = self.directory / 'foreign.pid'
        self.foreign = subprocess.Popen([sys.executable, str(self.child), str(self.foreign_pid)],
                                        start_new_session=True)
        self.runners.append(self.foreign)
        self.read_pid(self.foreign_pid)
        fixture = self.directory / 'fixture.sh'
        fixture.write_text('#!/bin/sh\ntrap "exit 0" TERM\n'
                           '"$TEST_PYTHON" "$TEST_CHILD" "$TEST_PIDFILE" &\n'
                           'while [ ! -s "$TEST_PIDFILE" ]; do sleep 0.01; done\n'
                           '[ "$1" = normal ] && exit 0\nwait\n')
        fixture.chmod(0o700)
        functions = SOURCE.read_text()
        selected = []
        for name in ('Die', 'KillRunning', 'RunWithTimeout'):
            match = re.search(r'^' + name + r'\(\) \{\n.*?^\}\n', functions, re.M | re.S)
            self.assertIsNotNone(match, name)
            selected.append(match.group())
        helper = self.directory / 'runner.sh'
        helper.write_text('set -u\nset -m\nRUNNING_GROUPS=""\nTIMEOUT_S=1\n' +
                          '\n'.join(selected) +
                          '\ntrap \'KillRunning; exit 143\' TERM\n'
                          'RunWithTimeout "$TEST_FIXTURE" "$TEST_LOG" "$TEST_MARKER" "$1"\n'
                          'status=$?\nprintf "%s\\n" "$status"\nexit "$status"\n')
        self.helper = helper
        self.pidfile = self.directory / 'child.pid'
        self.marker = self.directory / 'timeout.marker'
        self.environment = dict(os.environ, TEST_PYTHON=sys.executable,
                                TEST_CHILD=str(self.child), TEST_PIDFILE=str(self.pidfile),
                                TEST_FIXTURE=str(fixture), TEST_LOG=str(self.directory / 'case.log'),
                                TEST_MARKER=str(self.marker))

    def read_pid(self, path):
        until = time.monotonic() + 5
        while time.monotonic() < until:
            if path.exists() and path.read_text().strip():
                pid = int(path.read_text())
                self.children.append(pid)
                return pid
            time.sleep(0.01)
        detail = ''
        owner = self.runners[-1]
        if owner.poll() is not None:
            stdout, stderr = owner.communicate()
            detail = f'; owner status={owner.returncode}, stdout={stdout!r}, stderr={stderr!r}'
        case_log = self.directory / 'case.log'
        if case_log.exists():
            detail += '; case=' + case_log.read_text(errors='replace')[-1000:]
        self.fail('fixture did not publish its PID' + detail)

    def start(self, mode):
        runner = subprocess.Popen(['sh', str(self.helper), mode], env=self.environment,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  start_new_session=True, text=True)
        self.runners.append(runner)
        child = self.read_pid(self.pidfile)
        return runner, child

    def assert_clean(self, runner, child):
        stdout, stderr = runner.communicate(timeout=10)
        self.assertFalse(alive(child), 'TERM-ignoring descendant survived: ' + stderr)
        self.assertIsNone(self.foreign.poll(), 'foreign process was killed')
        return stdout, stderr

    def test_timeout_cleans_descendants_before_return(self):
        runner, child = self.start('timeout')
        stdout, stderr = self.assert_clean(runner, child)
        self.assertEqual(runner.returncode, 0, stderr)
        self.assertTrue(self.marker.exists(), stdout + stderr)

    def test_normal_owner_exit_cleans_descendants(self):
        runner, child = self.start('normal')
        stdout, stderr = self.assert_clean(runner, child)
        self.assertEqual(runner.returncode, 0, stdout + stderr)
        self.assertFalse(self.marker.exists())

    def test_interrupt_cleans_descendants(self):
        runner, child = self.start('timeout')
        runner.terminate()
        self.assert_clean(runner, child)
        self.assertEqual(runner.returncode, 143)

    def tearDown(self):
        for runner in self.runners:
            if runner.poll() is None:
                try:
                    os.killpg(runner.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            runner.wait(timeout=5)
            for stream in (runner.stdout, runner.stderr):
                if stream:
                    stream.close()
        for pid in self.children:
            if alive(pid):
                try:
                    os.kill(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
        self.temp.cleanup()


if __name__ == '__main__':
    unittest.main()
