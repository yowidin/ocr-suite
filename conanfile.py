from conans import ConanFile, CMake


class Recipe(ConanFile):
    name = 'ocr-suite'
    version = '0.0.1'

    description = 'OCR Suite'
    settings = 'os', 'arch', 'compiler', 'build_type'

    generators = 'cmake'

    requires = [
        'ffmpeg/5.0',
        'tesseract/5.2.0',
        'libpng/1.6.39',
        'catch2/3.2.1',
        'spdlog/1.9.2',
        'lyra/1.6.1',
        'sqlite3/3.40.0',
        'indicators/2.2',
        'boost/1.80.0',
    ]

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def configure(self):
        # JPEG and libx264 are breaking the build on Ubuntu
        self.options['leptonica'].with_jpeg=False
        self.options['leptonica'].with_openjpeg=False
        self.options['libtiff'].jpeg=False
        self.options['ffmpeg'].with_libx264=False
        self.options['ffmpeg'].with_libx265=False
        self.options['ffmpeg'].with_openjpeg=False
        self.options['tesseract'].with_auto_optimize=True
        self.options['tesseract'].with_march_native=False
