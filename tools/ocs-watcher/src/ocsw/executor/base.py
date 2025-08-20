import sys
import signal
import subprocess

from pathlib import Path

from ocsw.config import Config
from ocsw.config import INPUT_FILE_PLACEHOLDER, INPUT_FILE_STEM_PLACEHOLDER


class ExecutorBase:
    def __init__(self, config: Config, dry_run: bool):
        self.config = config
        self.dry_run = dry_run

        if sys.platform == 'win32':
            self.creation_flags = subprocess.CREATE_NEW_PROCESS_GROUP
            self.interrupt_signal = signal.CTRL_C_EVENT
        else:
            self.creation_flags = 0
            self.interrupt_signal = signal.SIGINT

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
