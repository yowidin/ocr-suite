from pydantic import ValidationError

from ocsw.options import Options
from ocsw.runner import Runner
from ocsw.single_instance import SingleInstance

import sys


def report_exception(e):
    print(e)
    sys.exit(-1)


def main():
    try:
        with SingleInstance():
            options = Options.get_options()
            runner = Runner(options)
            runner.run()
    except ValidationError as e:
        report_exception(f'Validation Error: {e}')
    except KeyboardInterrupt:
        report_exception(f'Keyboard interrupt')
    except OSError as e:
        report_exception(f'OS Error: {e}')
    except RuntimeError as e:
        report_exception(f'Runtime Error: {e}')


if __name__ == '__main__':
    main()
