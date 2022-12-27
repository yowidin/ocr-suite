import argparse
import os


class Options:
    def __init__(self, args):
        self.ocr_suite = args.ocr_suite
        self.ocr_lang = args.ocr_lang
        self.tess_data_path = args.tess_data_path
        self.num_ocr_threads = args.num_ocr_threads

        self.video_dir = args.video_dir
        self.check_frequency = args.checking_frequency
        self.file_extension = args.file_extension

    @staticmethod
    def get_options():
        num_threads = max(os.cpu_count() - 2, 1)

        parser = argparse.ArgumentParser()
        parser.add_argument('-o', '--ocr-suite', required=True, help='Path to the ocr-suite executable')
        parser.add_argument('-i', '--video-dir', required=True, help='Video files directory')
        parser.add_argument('-t', '--tess-data-path', required=True,
                            help='Path to the Tesseract data directory. You can download a copy here: '
                                 'https://github.com/tesseract-ocr/tessdata/releases/')
        parser.add_argument('-p', '--num-ocr-threads', default=num_threads, help='Number of OCR threads')
        parser.add_argument('-l', '--ocr-lang', default='eng+rus+deu',
                            help="OCR language, e.g. 'eng+rus+deu', or just 'eng'")
        parser.add_argument('-e', '--file-extension', type=str, default='mkv', help='Video files extension')
        parser.add_argument('-f', '--checking-frequency', type=int, default=10,
                            help='Video recognition frequency in minutes (can be used to recognize a vidio, '
                                 'which is still being recorded).')

        args = parser.parse_args()
        return Options(args)

    def __str__(self):
        params = {
            'ocr_suite': self.ocr_suite,
            'video_dir': self.video_dir,
            'tess_data_path': self.tess_data_path,
            'num_ocr_threads': self.num_ocr_threads,
            'ocr_lang': self.ocr_lang,
            'check_frequency': self.check_frequency,
            'file_extension': self.file_extension,
        }
        return str(params)

    def __repr__(self):
        return self.__str__()
