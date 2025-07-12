from conan import ConanFile
from conan.tools.files import copy
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout

from typing import List

import os


class Recipe(ConanFile):
    name = 'ocr-suite'
    version = '0.2.0'

    description = 'OCR Suite'
    settings = 'os', 'arch', 'compiler', 'build_type'

    requires = [
        # Main app
        'ffmpeg/7.1.1',
        'tesseract/5.5.0',
        'libpng/1.6.50',
        'catch2/3.2.1',
        'spdlog/1.9.2',
        'lyra/1.6.1',
        'sqlite-burrito/0.2.2@conan-burrito/stable',
        'indicators/2.2',
        'boost/1.88.0',

        # Viewer UI
        'sdl/2.26.1',
        'imgui/cci.20220621+1.88.docking',
        'glad/0.1.36'
    ]

    keep_imports = True

    def requirements(self):
        # Override incompatible dependencies
        self.requires('openssl/1.1.1t', override=True)
        self.requires('xz_utils/5.4.0', override=True)

        # The default zlib 1.2.13 (required by ffmpeg, boost, and other packets) is not buildable on macOS 15.5
        self.requires('zlib/1.3.1', override=True)

    def configure(self):
        # JPEG and libx264 are breaking the build on Ubuntu
        self.options['leptonica'].with_jpeg = False
        self.options['leptonica'].with_openjpeg = False

        self.options['libtiff'].jpeg = False

        self.options['ffmpeg'].with_libx264 = False
        self.options['ffmpeg'].with_libx265 = False
        self.options['ffmpeg'].with_openjpeg = False


        self.options['tesseract'].with_auto_optimize = True
        self.options['tesseract'].with_march_native = False

        # UI Tool settings
        self.options['glad'].spec = 'gl'
        self.options['glad'].gl_profile = 'core'
        self.options['glad'].gl_version = '3.2'

        # Turn off the Pulse Audio support for non-windows OSs
        if self.settings.os != 'Windows':
            self.options['ffmpeg'].with_pulse = False
            self.options['sdl'].pulse = False

        if self.settings.os == 'Macos':
            self.options['boost'].with_stacktrace_backtrace = False


    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def _import_bindings(self, target_dir):
        if not os.path.exists(target_dir):
            os.makedirs(target_dir)

        for dep in self.dependencies.values():
            srcdirs = dep.cpp_info.srcdirs  # type: List[str]
            if len(srcdirs) == 0:
                continue

            for src_dir in srcdirs:
                print(src_dir)
                copy(self, pattern='*.h', dst=target_dir, src=src_dir)
                copy(self, pattern='*.cpp', dst=target_dir, src=src_dir)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

        cmake_deps = CMakeDeps(self)
        cmake_deps.generate()

        self._import_bindings(target_dir=os.path.join(self.build_folder, 'bindings'))

    def package(self):
        cmake = CMake(self)
        cmake.install()
