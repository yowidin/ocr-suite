import subprocess
import sys
import signal

from pathlib import Path
from time import sleep, monotonic

from ocsw.config import Config
from ocsw.watcher import Watcher
from ocsw.file_filter import collect_by_extension


class Runner:
    SLEEP_DURATION = 5
    OUTPUT_POLLING_INTERVAL = 5

    def __init__(self, config: Config, dry_run: bool):
        self._config = config
        self._should_run = False
        self._last_check_time = 0.0
        self._dry_run = dry_run
        self._checking_frequency_seconds = self._config.checking_frequency * 60

    def _on_file_change(self, path):
        print('File change:', path)
        self._should_run = True

    def run(self):
        with Watcher(self._config, self._on_file_change):
            try:
                can_continue = self._process_all_files()
                while can_continue:
                    sleep(Runner.SLEEP_DURATION)

                    if self._should_run:
                        can_continue = self._process_all_files()
                        continue

                    current_time = monotonic()
                    if current_time - self._last_check_time > self._checking_frequency_seconds:
                        can_continue = self._process_all_files()
                        continue

            except KeyboardInterrupt:
                pass

    def _process_all_files(self) -> bool:
        files = collect_by_extension(self._config.watch_directory, self._config.file_extensions)
        can_continue = True
        for file in files:
            can_continue = self._process_one_file(file)
            if not can_continue:
                break

        self._should_run = False
        self._last_check_time = monotonic()

        return can_continue

    def _prepare_call_arguments(self, input_file: Path) -> list[str]:
        from ocsw.config import INPUT_FILE_PLACEHOLDER, INPUT_FILE_STEM_PLACEHOLDER

        stem_path = self._get_stem_path(input_file)

        args = [str(self._config.target_app)]
        for arg in self._config.call_arguments:
            if INPUT_FILE_PLACEHOLDER in arg:
                args.append(arg.replace(INPUT_FILE_PLACEHOLDER, str(input_file)))
                continue

            if INPUT_FILE_STEM_PLACEHOLDER in arg:
                args.append(arg.replace(INPUT_FILE_STEM_PLACEHOLDER, str(stem_path)))
                continue

            args.append(arg)

        return args

    @staticmethod
    def _get_stem_path(file_path: Path) -> Path:
        directory = file_path.parent
        file_name = file_path.stem
        return directory / file_name

    def _process_one_file(self, path: Path) -> bool:
        flags = 0
        if sys.platform == 'win32':
            flags = subprocess.CREATE_NEW_PROCESS_GROUP
            interrupt_signal = signal.CTRL_C_EVENT
        else:
            interrupt_signal = signal.SIGINT

        args = self._prepare_call_arguments(path)
        proc = subprocess.Popen(args, creationflags=flags, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

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
            if not self._dry_run:
                run_for_output()
        except KeyboardInterrupt:
            print(f'\nInterrupting OCR for {path}')
            proc.send_signal(interrupt_signal)
            run_for_output()
            return False

        return True
