from time import sleep, monotonic

from ocsw import console
from ocsw.config import Config
from ocsw.watcher import Watcher
from ocsw.file_filter import collect_by_extension
from ocsw.executor import Executor


class Runner:
    SLEEP_DURATION = 5
    OUTPUT_POLLING_INTERVAL = 5

    def __init__(self, config: Config):
        self._config = config
        self._should_run = False
        self._last_check_time = 0.0
        self._checking_frequency_seconds = self._config.checking_frequency * 60
        self._executor = Executor(config=config)

    def _on_file_change(self, path):
        console.log(f'File change: {path}')
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

        can_continue = self._executor.execute(files)

        self._should_run = False
        self._last_check_time = monotonic()

        return can_continue
