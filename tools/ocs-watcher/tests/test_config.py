import pytest
from pathlib import Path
from tempfile import NamedTemporaryFile, mkstemp

from pydantic import ValidationError

from ocsw.config import Config, MIN_CHECK_FREQUENCY, MAX_CHECK_FREQUENCY, DEFAULT_CHECK_FREQUENCY
from ocsw.config import Config, MIN_PARALLELISM, MAX_PARALLELISM, DEFAULT_PARALLELISM
from ocsw.config import DEFAULT_LOCK_NAME

import os


class ConfigPlaceholder:
    def __init__(self):
        fd, path = mkstemp()
        os.close(fd)

        self._tmp_file = path

        self.watch_directory = Path(path).parent
        self.file_extensions = ['.mkv', '.mp4', '.avi', '.mov']
        self.target_app = self._tmp_file
        self.call_arguments = ['-i', '%file_path%']
        self.checking_frequency = 20
        self.max_parallelism = 1
        self.lock_name = 'test.lock'

    def cleanup(self):
        if os.path.exists(self._tmp_file):
            os.remove(self._tmp_file)

    def as_dict(self):
        return {
            'watch_directory': self.watch_directory,
            'file_extensions': self.file_extensions,
            'target_app': self.target_app,
            'call_arguments': self.call_arguments,
            'checking_frequency': self.checking_frequency,
            'max_parallelism': self.max_parallelism,
            'lock_name': self.lock_name,
        }


@pytest.fixture
def valid_config():
    config = ConfigPlaceholder()
    yield config
    config.cleanup()


def test_config_happy_path(valid_config):
    _ = Config(**valid_config.as_dict())


def test_config_fails_on_invalid_watch_dir(valid_config):
    valid_config.watch_directory = '/invalid'
    with pytest.raises(ValidationError):
        _ = Config(**valid_config.as_dict())


def test_config_fails_on_empty_extensions_list(valid_config):
    valid_config.file_extensions = []
    with pytest.raises(ValidationError):
        _ = Config(**valid_config.as_dict())


def test_config_fails_on_invalid_extension(valid_config):
    valid_config.file_extensions = ['mkv']
    with pytest.raises(ValidationError):
        _ = Config(**valid_config.as_dict())


def test_config_fails_on_non_file_target_app(valid_config):
    valid_config.target_app = valid_config.watch_directory
    with pytest.raises(ValidationError):
        _ = Config(**valid_config.as_dict())


def test_config_fails_on_non_existing_target_app(valid_config):
    valid_config.target_app = '/invalid-app'
    with pytest.raises(ValidationError):
        _ = Config(**valid_config.as_dict())


def test_config_fails_on_empty_call_arguments_list(valid_config):
    valid_config.call_arguments = []
    with pytest.raises(ValidationError):
        _ = Config(**valid_config.as_dict())


def test_config_fails_on_call_arguments_list_missing_file_placeholder(valid_config):
    valid_config.call_arguments = ['-i']
    with pytest.raises(ValidationError):
        _ = Config(**valid_config.as_dict())


def test_config_checking_frequency_min_frequency_is_acceptable(valid_config):
    valid_config.checking_frequency = MIN_CHECK_FREQUENCY
    _ = Config(**valid_config.as_dict())


def test_config_checking_frequency_max_frequency_is_acceptable(valid_config):
    valid_config.checking_frequency = MAX_CHECK_FREQUENCY
    _ = Config(**valid_config.as_dict())


def test_config_fails_on_checking_frequency_too_small(valid_config):
    valid_config.checking_frequency = MIN_CHECK_FREQUENCY - 1
    with pytest.raises(ValidationError):
        _ = Config(**valid_config.as_dict())


def test_config_fails_on_checking_frequency_too_big(valid_config):
    valid_config.checking_frequency = MAX_CHECK_FREQUENCY + 1
    with pytest.raises(ValidationError):
        _ = Config(**valid_config.as_dict())


def test_config_missing_checking_frequency_is_default_frequency(valid_config):
    valid_config.checking_frequency = None
    cfg = Config(**valid_config.as_dict())
    assert cfg.checking_frequency == DEFAULT_CHECK_FREQUENCY


def test_config_max_parallelism_min_value_is_acceptable(valid_config):
    valid_config.max_parallelism = MIN_PARALLELISM
    _ = Config(**valid_config.as_dict())


def test_config_max_parallelism_max_value_is_acceptable(valid_config):
    valid_config.max_parallelism = MAX_PARALLELISM
    _ = Config(**valid_config.as_dict())


def test_config_max_parallelism_value_too_small(valid_config):
    valid_config.max_parallelism = MIN_PARALLELISM - 1
    with pytest.raises(ValidationError):
        _ = Config(**valid_config.as_dict())


def test_config_max_parallelism_value_too_big(valid_config):
    valid_config.max_parallelism = MAX_PARALLELISM + 1
    with pytest.raises(ValidationError):
        _ = Config(**valid_config.as_dict())


def test_config_missing_max_parallelism_is_default_value(valid_config):
    valid_config.max_parallelism = None
    cfg = Config(**valid_config.as_dict())
    assert cfg.max_parallelism == DEFAULT_PARALLELISM


def test_config_missing_lock_name_is_default_lock_name(valid_config):
    valid_config.lock_name = None
    cfg = Config(**valid_config.as_dict())
    assert cfg.lock_name == DEFAULT_LOCK_NAME


def test_config_fails_with_extra_args(valid_config):
    as_dict = valid_config.as_dict()
    as_dict['dummy-extra'] = 42
    with pytest.raises(ValidationError):
        _ = Config(**as_dict)


def test_config_toml_works_on_happy_path(valid_config):
    with NamedTemporaryFile(mode='w', delete_on_close=False) as f:
        f.write(f"""
watch_directory="{valid_config.watch_directory}"
file_extensions={repr(valid_config.file_extensions)}
target_app="{valid_config.target_app}"
call_arguments={repr(valid_config.call_arguments)}
checking_frequency={valid_config.checking_frequency}
lock_name="{valid_config.lock_name}"
""")
        f.close()
        path = Path(f.name)
        from_toml = Config.from_toml_file(path)

        direct = Config(**valid_config.as_dict())
        assert direct == from_toml
