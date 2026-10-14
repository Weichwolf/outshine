"""Real native blocking, worker retirement/recovery and unchanged pinned roof products."""

import ctypes
import json
import os
from pathlib import Path
import threading
import time

import numpy as np
import shapely

from evidence import revision
import outshine_ref.buildings.modelling.hip_worker as worker
from hip_worker_wire import receive,send
from outshine_ref.buildings.modelling import SkeletonBudgetExceeded,SkeletonLimits
from verify_hip_fixture import verify


def native_stall(connection):
    send(connection,('ready',os.getpid()));receive(connection);send(connection,('entered',None))
    ctypes.CDLL(None).pause()


def absent(pid):
    try:os.kill(pid,0)
    except ProcessLookupError:return True
    return False


def controls():
    polygon=shapely.box(0,0,12,5);record={};saved=worker.worker_loop
    worker.worker_loop=native_stall
    try:
        with worker.HipPlanner(SkeletonLimits(seconds=.2)) as planner:
            pid=planner.pid;started=time.monotonic()
            try:planner.prepare(polygon)
            except SkeletonBudgetExceeded:pass
            else:raise AssertionError('Blocked native call returned without timeout')
            elapsed=time.monotonic()-started
            assert planner.last_worker_entered and planner.last_exit_code<0 and absent(pid)
            record['timeout']=dict(elapsed_ms=elapsed*1000,pid=pid,exit_code=planner.last_exit_code,entered_native_worker=True,reaped=True)
            worker.worker_loop=saved
            plan=planner.prepare(polygon);assert planner.pid!=pid
            for array in [plan.positions_m,plan.distances_m]:
                try:array.flags.writeable=True
                except ValueError:pass
                else:raise AssertionError('Process transfer made plan data mutable')
            record['recovered']=dict(pid=planner.pid,facets=len(plan.facets),immutable=True)
        assert planner.pid is None
    finally:worker.worker_loop=saved
    worker.worker_loop=native_stall
    try:
        with worker.HipPlanner() as planner:
            errors=[]
            def request():
                try:planner.prepare(polygon)
                except Exception as error:errors.append(error)
            thread=threading.Thread(target=request);thread.start();started=time.monotonic()
            while not planner.last_worker_entered and thread.is_alive() and time.monotonic()-started<1.:time.sleep(.002)
            try:planner.prepare(polygon)
            except worker.HipWorkerError:pass
            else:raise AssertionError('Worker accepted a second concurrent request')
            pid=planner.pid;started=time.monotonic();assert planner.cancel();thread.join(1.)
            assert not thread.is_alive() and len(errors)==1 and isinstance(errors[0],worker.HipWorkerCancelled)
            assert absent(pid);record['cancel']=dict(elapsed_ms=(time.monotonic()-started)*1000,pid=pid,reaped=True)
    finally:worker.worker_loop=saved
    return record


def fixtures():
    import verify_hip_fixture as fixture_checker
    data=json.loads((Path(__file__).parent/'reference/hip-plans.json').read_text());original=fixture_checker.prepare_hip
    times=[];started=time.monotonic()
    with worker.HipPlanner() as planner:
        startup_ms=(time.monotonic()-started)*1000;pid=planner.pid
        fixture_checker.prepare_hip=planner.prepare
        try:
            for row in data['records']:
                started=time.monotonic();verify(row,data['comparison']);times.append((time.monotonic()-started)*1000)
                assert planner.pid==pid
        finally:fixture_checker.prepare_hip=original
    assert absent(pid)
    return dict(cases=len(times),startup_ms=startup_ms,p50_fixture_verify_ms=float(np.percentile(times,50)),
        p95_fixture_verify_ms=float(np.percentile(times,95)),worker_pid=pid,reaped=True)


if __name__=='__main__':
    result=revision()|dict(controls=controls(),fixtures=fixtures(),scope='Desktop spawn/socket prototype; native libc.pause timeout/cancel, child reaping, immutable recovery and unchanged 48-source fixture. Not an iOS or CGAL execution-time proof.')
    Path('build/experiments/buildings/hip-worker-verification.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ['implementation_sha256']},indent=2))
