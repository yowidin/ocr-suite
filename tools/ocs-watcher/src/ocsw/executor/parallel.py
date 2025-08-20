import sys
import signal
import subprocess

from concurrent.futures import ThreadPoolExecutor, as_completed
from threading import Lock
from pathlib import Path

from ocsw.executor.base import ExecutorBase


class ParallelExecutor(ExecutorBase):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)

        self._running_lock = Lock()
        self._running_set: set[subprocess.Popen] = set()

    def execute(self, files: list[Path]) -> bool:
        """
        Process a list of files in parallel.
        Returns False if interrupted by the user, True otherwise.
        """
        if self.dry_run:
            return True

        interrupted = False

        executor = ThreadPoolExecutor(max_workers=self.config.max_parallelism)
        futures = []

        try:
            for file in files:
                futures.append(executor.submit(self._run_one, file))

            for _ in as_completed(futures):
                pass

            executor.shutdown(wait=True)
        except KeyboardInterrupt:
            interrupted = True
            self._signal_all()

            for f in futures:
                f.cancel()

            executor.shutdown(wait=False, cancel_futures=True)
        except Exception:
            executor.shutdown(wait=False, cancel_futures=True)
            raise

        return not interrupted

    def _register(self, proc: subprocess.Popen) -> None:
        with self._running_lock:
            self._running_set.add(proc)

    def _unregister(self, proc: subprocess.Popen) -> None:
        with self._running_lock:
            self._running_set.discard(proc)

    def _signal_all(self) -> None:
        with self._running_lock:
            procs = list(self._running_set)

        for p in procs:
            try:
                if p.poll() is None:
                    p.send_signal(self.interrupt_signal)
            except Exception as e:
                print(f'Error sending interrupt signal: {e}', file=sys.stderr)

    def _run_one(self, path: Path) -> int:
        args = self.make_subprocess_args(path)

        print(f'Handling {path}: {" ".join(args)}')

        proc = subprocess.Popen(args, creationflags=self.creation_flags, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                text=True)
        self._register(proc)
        try:
            _, err = proc.communicate()
            if proc.returncode != 0:
                err_str = (err or '').strip()
                if err_str:
                    print(f'Runner errors for {path}:\n{err_str}', flush=True)
            return proc.returncode or 0
        finally:
            self._unregister(proc)
