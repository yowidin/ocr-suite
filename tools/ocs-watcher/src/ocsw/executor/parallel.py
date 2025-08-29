import sys
import re
import subprocess

from concurrent.futures import ThreadPoolExecutor, as_completed
from threading import Lock
from pathlib import Path

from ocsw.executor.base import ExecutorBase

from rich.progress import (
    BarColumn,
    Progress,
    TaskID,
    TextColumn,
    TimeRemainingColumn
)
from rich.console import Console


class ParallelExecutor(ExecutorBase):
    PERCENTAGE_REGEX = r'.*?\s+(\d{1,2})%'

    def __init__(self, **kwargs):
        super().__init__(**kwargs)

        self._running_lock = Lock()
        self._running_set: set[subprocess.Popen] = set()

        self._progress = Progress(
            TextColumn("[bold blue]{task.fields[filename]}", justify="right"),
            BarColumn(bar_width=None),
            "[progress.percentage]{task.percentage:>3.1f}%",
            TimeRemainingColumn(compact=True, elapsed_when_finished=True),
            console=Console(log_path=False),
        )

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
            with self._progress:
                for file in files:
                    task_id = self._progress.add_task("Processing", filename=file.name, start=False)
                    futures.append(executor.submit(self._run_one, file, task_id))

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

    def _register(self, proc: subprocess.Popen, task_id: TaskID, path: Path) -> None:
        with self._running_lock:
            self._running_set.add(proc)
            self._progress.log(f"Starting {path}")
            self._progress.update(task_id, total=100)

    def _unregister(self, proc: subprocess.Popen, task_id: TaskID, path: Path, finished: bool) -> None:
        with self._running_lock:
            self._running_set.discard(proc)

            if finished:
                self._progress.log(f"Finished {path}")
            else:
                self._progress.log(f"Interrupted {path}")

            self._progress.update(task_id, completed=100)

    def _update_progress(self, task_id: TaskID, progress: int) -> None:
        with self._running_lock:
            self._progress.update(task_id, completed=progress)

    def _signal_all(self) -> None:
        with self._running_lock:
            procs = list(self._running_set)

        for p in procs:
            try:
                if p.poll() is None:
                    p.send_signal(self.interrupt_signal)
            except Exception as e:
                print(f'Error sending interrupt signal: {e}', file=sys.stderr)

    def _run_one(self, path: Path, task_id: TaskID) -> int:
        args = self.make_subprocess_args(path)

        print(f'Handling {path}: {" ".join(args)}')

        last_percentage = -1

        def check_percentage_updates(data: bytes):
            decoded = data.decode('utf-8')
            nonlocal last_percentage
            try:
                matches = re.findall(self.PERCENTAGE_REGEX, decoded)
            except Exception as e:
                matches = []
                print(f'Error parsing percentage: {e}', file=sys.stderr)

            if not matches:
                return

            match = matches[-1]
            percentage = int(match)
            if percentage > last_percentage:
                self._update_progress(task_id, percentage)
                last_percentage = percentage

        proc = subprocess.Popen(args, creationflags=self.creation_flags, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                text=True)
        self._register(proc, task_id, path)
        finished = False
        try:
            while proc.poll() is None:
                try:
                    out, err = proc.communicate(timeout=1)
                    if proc.returncode != 0:
                        err_str = (err or '').strip()
                        if err_str:
                            print(f'Runner errors for {path}:\n{err_str}', flush=True)

                    check_percentage_updates(out or b'')
                    finished = True
                    return proc.returncode or 0
                except subprocess.TimeoutExpired as e:
                    check_percentage_updates(e.stdout or b'')

        finally:
            self._unregister(proc, task_id, path, finished)
