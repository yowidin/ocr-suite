#!/usr/bin/env python3

import os
from subprocess import run
from sys import platform

source_dir = os.getcwd()
build_dir = os.path.join(source_dir, "build", "release")
if not os.path.exists(build_dir):
    os.makedirs(build_dir)

kwargs = {'cwd': build_dir, 'check': True}
install_args = ['conan', 'install', source_dir, '-b', 'missing', '-s', 'compiler.cppstd=17', '-s', 'build_type=Release']

if platform == 'darwin':
    # Build for High Sierra
    install_args.extend(['-s', 'os.version=10.13'])

run(install_args, **kwargs)
run(['cmake', '--preset', 'release', source_dir], **kwargs)
run(['cmake', '--build', '--preset', 'release', source_dir], **kwargs)
run(['cpack', '-G', 'ZIP'], **kwargs)
