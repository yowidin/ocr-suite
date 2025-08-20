from pathlib import Path
from typing import Annotated

from pydantic import BaseModel, ConfigDict, StringConstraints, FilePath, DirectoryPath, AfterValidator
from annotated_types import MinLen, Interval

import tomlkit
import os

# A file extension starts with '.', consists of words and digits and can be anywhere between 1 and 6 characters long
type FileExtension = Annotated[str, StringConstraints(pattern=r'^\.(?:\w|\d){1,6}$')]

type NonEmptyString = Annotated[str, MinLen(1)]

INPUT_FILE_PLACEHOLDER = "%file_path%"

# Same ase input file path but without the file extension
INPUT_FILE_STEM_PLACEHOLDER = "%file_stem_path%"

MIN_CHECK_FREQUENCY = 5
MAX_CHECK_FREQUENCY = 60 * 8
DEFAULT_CHECK_FREQUENCY = 10

MIN_PARALLELISM = 1
MAX_PARALLELISM = os.cpu_count() * 4
DEFAULT_PARALLELISM = 1

DEFAULT_LOCK_NAME = 'ocsw.lock'


def validate_call_arguments(v: list[str]) -> list[str]:
    if INPUT_FILE_PLACEHOLDER not in v:
        raise ValueError('call_arguments should contain a file placeholder: %file_path%')
    return v


class Config(BaseModel):
    model_config = ConfigDict(extra="forbid")

    # The directory to recursively check the files in
    watch_directory: DirectoryPath

    # File extensions to be checked
    file_extensions: Annotated[list[FileExtension], MinLen(1)]

    # Application to be called on each file with matching extension
    target_app: FilePath

    # Arguments to be passed to the target app, use %file_path% as a placeholder for passing
    # the file path found inside the watch directory.
    call_arguments: Annotated[list[NonEmptyString], MinLen(1), AfterValidator(validate_call_arguments)]

    # How frequently should all the files be rechecked.
    # This means that if a frequency is specified, then the target app should be re-entrant.
    checking_frequency: Annotated[int, Interval(ge=MIN_CHECK_FREQUENCY,
                                                le=MAX_CHECK_FREQUENCY)] | None = DEFAULT_CHECK_FREQUENCY

    # Lock file name: doesn't allow running multiple instances of the same application
    lock_name: NonEmptyString | None = DEFAULT_LOCK_NAME

    # Maximum parallelism: how many files can be processed simultaneously
    # Setting this to a value greater than 1 will suppress the standard output for the files being processed.
    # In that case only the errors will be printed.
    max_parallelism: Annotated[int, Interval(ge=MIN_PARALLELISM,
                                             le=MAX_PARALLELISM)] | None = DEFAULT_PARALLELISM

    def __init__(self, **kwargs):
        super().__init__(**kwargs)

        if self.checking_frequency is None:
            self.checking_frequency = DEFAULT_CHECK_FREQUENCY

        if self.max_parallelism is None:
            self.max_parallelism = DEFAULT_PARALLELISM

        if self.lock_name is None:
            self.lock_name = DEFAULT_LOCK_NAME

    @staticmethod
    def from_toml_file(path: Path) -> 'Config':
        file_contents = path.read_text()
        as_toml = tomlkit.loads(file_contents)
        as_dict = as_toml.unwrap()
        result = Config.model_validate(as_dict)
        return result
