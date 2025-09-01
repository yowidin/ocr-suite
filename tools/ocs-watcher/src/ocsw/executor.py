import re
import sys
import signal
import subprocess
import threading
import select

from pathlib import Path

from concurrent.futures import ThreadPoolExecutor, as_completed

from ocsw import console
from ocsw.config import Config
from ocsw.config import INPUT_FILE_PLACEHOLDER, INPUT_FILE_STEM_PLACEHOLDER

from rich.progress import (
    BarColumn,
    Progress,
    TaskID,
    TextColumn,
)


class Executor:
    PERCENTAGE_REGEX = r'^.*?\s+(\d{1,2})%\s+\w+ ... (.*) OCR/s, (.*) seek/s, (.*) in queue \[(.*) / (.*)\]\s*$'
    NON_PERCENTAGE_REGEX = r'^.*?\w+ ... (.*) OCR/s, (.*) seek/s, (.*) in queue \[(.*)\]\s*$'
    STARTING_PERCENTAGE_REGEX = r'^.*?\s+(\d{1,2})%\s+Starting.*$'
    STOPPING_PERCENTAGE_REGEX = r'^.*?\s+(\d{1,2})%\s+Stopping.*$'

    def __init__(self, config: Config):
        self.config = config

        if sys.platform == 'win32':
            self.creation_flags = subprocess.CREATE_NEW_PROCESS_GROUP
            self.interrupt_signal = signal.CTRL_C_EVENT
        else:
            self.creation_flags = 0
            self.interrupt_signal = signal.SIGINT

        self._overall_progress = Progress(
            TextColumn("Overall Progress", justify="right"),
            BarColumn(bar_width=None),
            "[progress.percentage]{task.percentage:>3.1f}%",
            console=console,
        )
        self._overall_id = self._overall_progress.add_task("OCR", total=0)

        self._progress = Progress(
            TextColumn("[bold blue]{task.fields[filename]}", justify="right"),
            BarColumn(bar_width=None),
            "[progress.percentage]{task.percentage:>3.1f}%",
            TextColumn('{task.fields[ocr_per_second]:3.1f} OCR/s'),
            TextColumn('{task.fields[seek_per_second]:3.1f} S/s'),
            TextColumn('{task.fields[in_queue]:02d} Q'),
            TextColumn('{task.fields[time_info]}'),
            console=console,
        )
        self._percentage_regex = re.compile(self.PERCENTAGE_REGEX)
        self._non_percentage_regex = re.compile(self.NON_PERCENTAGE_REGEX)
        self._starting_percentage_regex = re.compile(self.STARTING_PERCENTAGE_REGEX)
        self._stopping_percentage_regex = re.compile(self.STOPPING_PERCENTAGE_REGEX)

    @staticmethod
    def _get_stem_path(file_path: Path) -> Path:
        directory = file_path.parent
        file_name = file_path.stem
        return directory / file_name

    def make_subprocess_args(self, input_file: Path) -> list[str]:
        stem_path = self._get_stem_path(input_file)

        args = [str(self.config.target_app)]
        for arg in self.config.call_arguments:
            if INPUT_FILE_PLACEHOLDER in arg:
                args.append(arg.replace(INPUT_FILE_PLACEHOLDER, str(input_file)))
                continue

            if INPUT_FILE_STEM_PLACEHOLDER in arg:
                args.append(arg.replace(INPUT_FILE_STEM_PLACEHOLDER, str(stem_path)))
                continue

            args.append(arg)

        return args

    def execute(self, files: list[Path]) -> bool:
        interrupted = False

        executor = ThreadPoolExecutor(max_workers=self.config.max_parallelism)

        futures = []

        try:
            with self._overall_progress:
                self._overall_progress.update(self._overall_id, total=len(files))

                with self._progress:
                    for file in sorted(files):
                        futures.append(executor.submit(self._run_one, file))

                    for _ in as_completed(futures):
                        self._overall_progress.update(self._overall_id, advance=1)

            executor.shutdown(wait=True)
        except KeyboardInterrupt:
            self._progress.log(f'Keyboard interrupt')
            interrupted = True
            executor.shutdown(wait=True, cancel_futures=True)

        return not interrupted

    def _run_one(self, path: Path):
        self._progress.log(f'{path}: [yellow]Start')

        task_id = self._progress.add_task("Processing", filename=path, total=100, ocr_per_second=0.0,
                                          seek_per_second=0.0, in_queue=0,
                                          time_info='[??:??:?? / ??:??:??]')

        args = self.make_subprocess_args(path)
        process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1,
                                   close_fds=True)

        thread = threading.Thread(target=self._monitor_output, args=(task_id, process, path))
        thread.daemon = True
        thread.start()

        try:
            process.wait()
        except Exception as e:
            self._progress.log(f'Error processing {path}: {str(e)}')
            process.terminate()
            process.wait()

        thread.join()

        success = process.returncode == 0
        if success:
            self._progress.log(f"{path}: [green]Done")
            self._progress.update(task_id, completed=100)
        else:
            self._progress.log(f"[bold red]Failed {path}")

        self._progress.remove_task(task_id)

    def _monitor_output(self, task_id: TaskID, process: subprocess.Popen, path: Path):
        def flush_remaining_output():
            if process.stdout:
                for l in process.stdout.readlines():
                    self._process_stdout(l, task_id, path)

            if process.stderr:
                for l in process.stderr.readlines():
                    self._process_stderr(l, path)

        try:
            while process.poll() is None:
                rlist, _, _ = select.select([process.stdout, process.stderr], [], [], self.config.max_parallelism)
                for fd in rlist:
                    if fd == process.stdout:
                        line = process.stdout.readline()
                        if line:
                            self._process_stdout(line, task_id, path)
                    elif fd == process.stderr:
                        line = process.stderr.readline()
                        if line:
                            self._process_stderr(line, path)

            flush_remaining_output()
        except Exception as e:
            self._progress.log(f'[bold red]Error monitoring subprocess: {e}')

        process.stdout.close()
        process.stderr.close()

    def _process_stdout(self, line: str, task_id: TaskID, path: Path):
        m = self._percentage_regex.match(line)
        if m:
            percentage = int(m.group(1))
            ocr_per_second = float(m.group(2))
            seek_per_second = float(m.group(3))
            in_queue = int(m.group(4))
            current_time = m.group(5)
            total_time = m.group(6)
            time_info = f'[{current_time} / {total_time}]'
            self._progress.update(task_id, completed=percentage, ocr_per_second=ocr_per_second,
                                  seek_per_second=seek_per_second, in_queue=in_queue, current_time=current_time,
                                  time_info=time_info)
            return

        m = self._non_percentage_regex.match(line)
        if m:
            percentage = 0
            ocr_per_second = float(m.group(1))
            seek_per_second = float(m.group(2))
            in_queue = int(m.group(3))
            current_time = m.group(4)
            time_info = f'[{current_time} / ??:??:??]'
            self._progress.update(task_id, completed=percentage, ocr_per_second=ocr_per_second,
                                  seek_per_second=seek_per_second, in_queue=in_queue, current_time=current_time,
                                  time_info=time_info)
            return

        m = self._starting_percentage_regex.match(line)
        if m:
            percentage = int(m.group(1))
            self._progress.update(task_id, completed=percentage)
            return

        m = self._stopping_percentage_regex.match(line)
        if m:
            percentage = int(m.group(1))
            self._progress.update(task_id, completed=percentage)
            return

        stripped = line.strip()

        to_skip = ['Stopping', 'Starting decoder', 'consumer threads', 'Done!']
        for pattern in to_skip:
            if pattern in stripped:
                return

        console.log(f'{path}: {line.strip()}')

    def _process_stderr(self, line: str, path: Path):
        self._progress.log(f"[bold red]{path}: {line.strip()}")
