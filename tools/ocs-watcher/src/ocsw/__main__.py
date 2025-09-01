from argparse import ArgumentParser
from pydantic import ValidationError
from pathlib import Path

from ocsw.runner import Runner
from ocsw.config import Config
from ocsw.single_instance import SingleInstance
from ocsw import console

import sys


def report_exception(e):
    console.log(e)
    sys.exit(-1)


def get_config() -> Config:
    default_config = Path('ocsw-config.toml').absolute()

    parser = ArgumentParser()
    parser.add_argument('--config', '-c', type=str, required=False, default=default_config, help="Configuration file")

    args = parser.parse_args()
    config_file = Path(args.config).absolute()

    if not config_file.exists():
        raise RuntimeError(f'Config not found: {config_file}')

    config = Config.from_toml_file(config_file)

    return config


def main():
    try:
        config = get_config()

        with SingleInstance(lock_name=config.lock_name):
            runner = Runner(config)
            runner.run()

    except ValidationError as e:
        report_exception(f'Validation Error: {e}')
    except KeyboardInterrupt:
        report_exception(f'Keyboard interrupt')
    except OSError as e:
        report_exception(f'{e}')
    except RuntimeError as e:
        report_exception(f'{e}')


if __name__ == '__main__':
    main()
