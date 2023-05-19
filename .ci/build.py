#!/usr/bin/env python3

import os
import sys
from subprocess import run
from sys import platform
from argparse import ArgumentParser


def do_run(*args, **kwargs):
    print(f"--- running: {' '.join(*args)} ---")
    sys.stdout.flush()
    run(*args, **kwargs)


def main():
    parser = ArgumentParser('Project builder')
    parser.add_argument('--preset', default='Release', help='Build preset')

    args = parser.parse_args()

    source_dir = os.getcwd()
    build_dir = os.path.join(source_dir, 'build', args.preset)

    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    build_args = ['conan', 'build', '-b', 'missing', '-s', 'compiler.cppstd=17', '-s', 'build_type=Release',
                  '-c:h', 'tools.cmake.cmake_layout:build_folder_vars=["settings.build_type"]', source_dir]
    if platform == 'darwin':
        # Build for High Sierra
        build_args.extend(['-s', 'os.version=10.13'])

    extra_kw = {'cwd': build_dir, 'check': True}
    do_run(build_args, **extra_kw)
    do_run(['cpack', '-G', 'ZIP'], **extra_kw)


if __name__ == "__main__":
    main()
