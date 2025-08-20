import subprocess
from pathlib import Path

from ocsw.executor.base import ExecutorBase


class SequentialExecutor(ExecutorBase):
    """
    Sequential executor.
    Processes files sequentially and prints live output for every file being processed.
    """

    OUTPUT_POLLING_INTERVAL = 5

    def __init__(self, **kwargs):
        super().__init__(**kwargs)

    def execute(self, files: list[Path]) -> bool:
        """
        Process a list of files sequentially.
        Returns False if interrupted by the user, True otherwise.
        """

        interrupted = False
        for file in files:
            interrupted = self._process_one_file(file)
            if interrupted:
                break

        return not interrupted

    def _process_one_file(self, path: Path) -> bool:
        """
        Process one file.
        :param path: Path to the file.
        :return: True if the user interrupted the process, False otherwise.
        """

        args = self.make_subprocess_args(path)

        proc = None
        if not self.dry_run:
            proc = subprocess.Popen(args, creationflags=self.creation_flags, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE)

        last_output = ''

        def get_optional_text(stream) -> str:
            return stream.decode('utf-8') if stream is not None else ''

        def handle_outputs(timeout):
            outs, errs = proc.communicate(timeout=timeout)
            out_text = get_optional_text(outs)
            if len(out_text) != 0:
                print(out_text, end='', sep='', flush=True)

            if proc.returncode != 0:
                error = errs.decode("utf-8").strip()
                if len(error) != 0:
                    print(f'Runner errors for {path}:\n{error}')

        def run_for_output():
            nonlocal last_output
            while proc.poll() is None:
                try:
                    handle_outputs(timeout=self.OUTPUT_POLLING_INTERVAL)
                except subprocess.TimeoutExpired as e:
                    text = get_optional_text(e.stdout)
                    if len(text) != 0:
                        print(text[len(last_output):], end='', sep='', flush=True)
                    last_output = text

        try:
            print(f'Handling {path}: {" ".join(args)}')
            if proc is not None:
                run_for_output()
        except KeyboardInterrupt:
            print(f'\nInterrupting worker for {path}')
            if proc is not None:
                proc.send_signal(self.interrupt_signal)
                run_for_output()
            return True

        return False
