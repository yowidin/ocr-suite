from typing import Callable

from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler, FileModifiedEvent

from ocsw.options import Options


class Watcher:

    class EventHandler(FileSystemEventHandler):
        def __init__(self, options: Options, change_callback: Callable[[str], None]):
            self._callback = change_callback
            self._file_extension = options.file_extension

        def on_modified(self, event):
            if not isinstance(event, FileModifiedEvent):
                # Ignore directory modifications (e.g.: journal file being created and removed)
                return

            path = event.src_path
            if path.endswith(self._file_extension):
                self._callback(path)

    def __init__(self, options: Options, change_callback: Callable[[str], None]):
        self._handler = Watcher.EventHandler(options, change_callback)
        self._observer = Observer()
        self._observer.schedule(self._handler, path=options.video_dir, recursive=False)

    def __enter__(self):
        self._observer.start()

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._observer.stop()
        self._observer.join()
