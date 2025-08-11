from ocsw.options import Options
from ocsw.runner import Runner


def main():
    options = Options.get_options()
    runner = Runner(options)
    runner.run()


if __name__ == '__main__':
    main()
