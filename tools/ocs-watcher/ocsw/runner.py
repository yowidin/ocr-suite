import subprocess
import sys
import os

from pathlib import Path
from time import sleep, monotonic

from ocsw.options import Options
from ocsw.watcher import Watcher


class Runner:
    SLEEP_DURATION = 5
    OUTPUT_POLLING_INTERVAL = 5

    def __init__(self, options: Options):
        self._options = options
        self._should_run = False
        self._last_check_time = 0.0
        self._check_frequency_seconds = self._options.check_frequency * 60

    def _on_file_change(self, path):
        print('File change:', path)
        self._should_run = True

    def run(self):

        with Watcher(self._options, self._on_file_change):
            try:
                can_continue = self._ocr_all_files()
                while can_continue:
                    sleep(Runner.SLEEP_DURATION)

                    if self._should_run:
                        can_continue = self._ocr_all_files()
                        continue

                    current_time = monotonic()
                    if current_time - self._last_check_time > self._check_frequency_seconds:
                        can_continue = self._ocr_all_files()
                        continue

            except KeyboardInterrupt:
                pass

    def _get_video_list(self):
        video_dir = self._options.video_dir
        extension = self._options.file_extension

        _, _, filenames = next(os.walk(video_dir))
        return [os.path.join(video_dir, x) for x in filenames if x.endswith(extension)]

    def _ocr_all_files(self) -> bool:
        files = self._get_video_list()
        can_continue = True
        for file in files:
            can_continue = self._ocr_file(file)
            if not can_continue:
                break

        self._should_run = False
        self._last_check_time = monotonic()

        return can_continue

    @staticmethod
    def _get_database_path(video_path):
        path = Path(video_path)

        directory = path.parent
        file_name = os.path.splitext(path.name)[0]

        return os.path.join(directory, file_name + '.db')

    def _ocr_file(self, path) -> bool:
        flags = 0
        if sys.platform == 'win32':
            flags = subprocess.CREATE_NEW_PROCESS_GROUP
            signal = subprocess.signal.CTRL_C_EVENT
        else:
            signal = subprocess.signal.SIGINT

        args = [
            self._options.ocr_suite,
            '-p', str(self._options.num_ocr_threads),
            '-l', self._options.ocr_lang,
            '-t', self._options.tess_data_path,
            '-i', path,
            '-o', self._get_database_path(path)
        ]
        proc = subprocess.Popen(args, creationflags=flags, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        def handle_outputs(timeout):
            outs, errs = proc.communicate(timeout=timeout)
            if outs is not None and len(outs) != 0:
                print(outs.decode('utf-8'), end='', sep='', flush=True)

            if proc.returncode != 0:
                error = errs.decode("utf-8").strip()
                if len(error) != 0:
                    print(f'OCR errors for {path}:\n{error}')
                return False

        def run_for_output():
            while proc.poll() is None:
                try:
                    handle_outputs(timeout=self.OUTPUT_POLLING_INTERVAL)
                except subprocess.TimeoutExpired as e:
                    print(e.stdout.decode('utf-8'), end='', sep='', flush=True)

        try:
            print(f'Running OCR for {path}')
            run_for_output()

        except KeyboardInterrupt:
            print(f'\nInterrupting OCR for {path}')
            proc.send_signal(signal)
            run_for_output()
            return False

        return True
