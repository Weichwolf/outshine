"""Explicit desktop roof worker; one request, bounded IO, kill/discard/reap on abort."""

from dataclasses import replace
import multiprocessing
import os
import socket
import threading
import time

import numpy as np
import shapely

from .hip_wire import HipWireError,receive,send
from .skeleton_plan import HipPlan,SkeletonBudgetExceeded,SkeletonLimits,prepare_hip
from .skeleton_plan import immutable_array


class HipWorkerError(RuntimeError):pass
class HipWorkerCancelled(SkeletonBudgetExceeded):pass


def worker_loop(connection):
    with open(os.devnull,'wb') as quiet:
        os.dup2(quiet.fileno(),1);os.dup2(quiet.fileno(),2)
    try:
        send(connection,('ready',os.getpid()))
        while True:
            request=receive(connection)
            send(connection,('entered',None))
            try:
                data,method,limits=request;plan=prepare_hip(shapely.from_wkb(data),method,limits)
                reply=('ok',plan)
            except Exception as error:reply=('error',(type(error).__name__,str(error)[:1000]))
            send(connection,reply)
    except (EOFError,OSError):pass
    finally:connection.close()


class HipPlanner:
    def __init__(self,limits=SkeletonLimits(),startup_seconds=5.,cleanup_seconds=.5):
        if not isinstance(limits,SkeletonLimits):raise ValueError('Invalid planner limits')
        if any(not np.isfinite(t) or t<=0 for t in [startup_seconds,cleanup_seconds]):raise ValueError('Invalid worker lifecycle budget')
        self.limits=limits;self.startup_seconds=startup_seconds;self.cleanup_seconds=cleanup_seconds
        self._process=None;self._connection=None;self._closed=False;self._active=False
        self._gate=threading.Lock();self._state=threading.Lock();self._cancelled=threading.Event()
        self.last_worker_entered=False;self.last_exit_code=None

    @property
    def pid(self):return self._process.pid if self._process is not None else None

    def _retire(self):
        process=self._process;connection=self._connection;self._connection=None
        if connection is not None:connection.close()
        if process is not None:
            if process.pid is not None:
                if process.is_alive():process.kill()
                process.join(self.cleanup_seconds)
            if process.is_alive():raise HipWorkerError('Worker cleanup remains pending')
            self.last_exit_code=process.exitcode;process.close();self._process=None

    def _start(self):
        if self._process is not None and self._process.is_alive() and self._connection is not None:return
        self._retire();parent,child=socket.socketpair()
        process=multiprocessing.get_context('spawn').Process(target=worker_loop,args=(child,),daemon=True)
        self._process=process;self._connection=parent
        try:
            process.start();child.close()
            result=receive(parent,time.monotonic()+self.startup_seconds)
            if result!=('ready',process.pid):raise HipWorkerError('Invalid worker readiness message')
        except BaseException:
            child.close();self._retire();raise

    def __enter__(self):
        if not self._gate.acquire(blocking=False):raise HipWorkerError('Planner already has an active request')
        try:
            if self._closed:raise HipWorkerError('Planner is closed')
            self._start()
            if self._closed:
                self._retire();raise HipWorkerError('Planner was closed during startup')
            return self
        finally:self._gate.release()

    def __exit__(self,*unused):self.close()

    def cancel(self):
        with self._state:
            if not self._active:return False
            self._cancelled.set()
            if self._process is not None:
                try:self._process.kill()
                except (OSError,ValueError,AttributeError):pass
            if self._connection is not None:
                try:self._connection.shutdown(socket.SHUT_RDWR)
                except OSError:pass
            return True

    def close(self):
        with self._state:self._closed=True
        self.cancel()
        if not self._gate.acquire(timeout=self.cleanup_seconds):raise HipWorkerError('Planner closure waiting for active owner')
        try:self._retire()
        finally:self._gate.release()

    def prepare(self,polygon,method='auto'):
        if not self._gate.acquire(blocking=False):raise HipWorkerError('Planner already has an active request')
        try:
            with self._state:
                if self._closed:raise HipWorkerError('Planner is closed')
                self._active=True;self._cancelled.clear();self.last_worker_entered=False
            if not isinstance(polygon,shapely.Polygon):raise ValueError('Roof planner requires a polygon')
            if method not in ('auto','skeleton','rectangle'):raise ValueError('Invalid roof planning method')
            count=sum(len(r.coords)-1 for r in (polygon.exterior,*polygon.interiors))
            if count>self.limits.vertices:raise SkeletonBudgetExceeded('Roof input vertex capacity exceeded')
            self._start();deadline=time.monotonic()+self.limits.seconds
            if self._cancelled.is_set():raise HipWorkerCancelled('Roof request cancelled')
            send(self._connection,(polygon.wkb,method,self.limits),deadline)
            result=receive(self._connection,deadline)
            if not isinstance(result,tuple) or len(result)!=2:raise HipWireError('Invalid roof response envelope')
            if result[0]=='entered':self.last_worker_entered=True;result=receive(self._connection,deadline)
            if not isinstance(result,tuple) or len(result)!=2:raise HipWireError('Invalid roof response envelope')
            if self._cancelled.is_set():raise HipWorkerCancelled('Roof request cancelled')
            if result[0]=='error':
                if not isinstance(result[1],tuple) or len(result[1])!=2:raise HipWireError('Invalid roof error response')
                kind,message=result[1];error={'ValueError':ValueError,'SkeletonBudgetExceeded':SkeletonBudgetExceeded,
                    'ModuleNotFoundError':ModuleNotFoundError}.get(kind,HipWorkerError);raise error(message)
            if result[0]!='ok' or not isinstance(result[1],HipPlan):raise HipWireError('Invalid roof worker response')
            plan=result[1]
            plan=replace(plan,positions_m=immutable_array(plan.positions_m),distances_m=immutable_array(plan.distances_m))
            if time.monotonic()>deadline:raise TimeoutError('Roof result decode exceeded request budget')
            return plan
        except (TimeoutError,EOFError,OSError,HipWorkerCancelled,HipWireError) as error:
            self._retire()
            if self._cancelled.is_set():raise HipWorkerCancelled('Roof request cancelled') from error
            if isinstance(error,TimeoutError):raise SkeletonBudgetExceeded('Roof process request deadline exceeded') from error
            raise HipWorkerError('Roof worker exited without a complete result') from error
        finally:
            with self._state:self._active=False
            self._gate.release()
