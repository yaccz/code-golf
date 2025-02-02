import dataclasses as dc
import time
import traceback as tb
from multiprocessing import Process, Queue, Event, Barrier

from jmcgmqp.core.primitives import WorkerResult, Results, SampleDescription
from jmcgmqp.core.event import Event as E
import jmcgmqp.mq_system as mqs

from . import abc

def worker(
    connector: mqs.abc.Connector,
    sdesc: SampleDescription,
    worker_id: int,
    q: Queue,
    exit_flag: Event,
    error: Event,
    b: Barrier
):
    try:
        sender = connector.connect()
    except Exception:
        tb.print_exc()
        error.set()
    finally:
        b.wait()

    start = time.time_ns()

    i = 0
    while not exit_flag.is_set():
        sender(i)
        i += 1

    end = time.time_ns()

    r = WorkerResult(sdesc, worker_id, i, end-start)
    q.put(r)

def check(error):
    if error.is_set():
        raise RuntimeError('Worker error')

@dc.dataclass
class Sampler(abc.Sampler):
    def sample(self, n):
        self.observable.publish(E.SamplingWorkers, n)
        q = Queue()
        exit_flag = Event()
        error = Event()
        b = Barrier(n+1)
        ps = []
        try:
            sdesc = SampleDescription(
                n,
                'multiprocessing',
                self.connector.name,
            )
            for i in range(1, n+1):
                check(error)
                p = Process(target=worker, args=(
                    self.connector, sdesc, i, q, exit_flag, error, b)
                )
                ps.append(p)
                try:
                    p.start()
                except:
                    error.set()
                    check(error)

            b.wait()

            self.observable.publish(E.WaitingInit, None)
            for i in range(self.app.config.DURATION, 0, -1):
                check(error)
                self.observable.publish(E.Waiting, i)
                time.sleep(1)

            exit_flag.set()
            for p in ps:
                p.join()

            check(error)
            xs = []
            for _ in range(0, n):
                wr = q.get()
                xs.append(wr)
                self.observable.publish(E.WorkerResult, wr)
            r = Results(xs)
            self.observable.publish(E.SampleResult, r)
            return r
        except:
            for p in ps:
                p.kill()
            raise
