from typing import Callable
from pathlib import Path

from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler, FileModifiedEvent

from ocsw.config import Config


class Watcher:
    class EventHandler(FileSystemEventHandler):
        def __init__(self, config: Config, change_callback: Callable[[Path], None]):
            self._callback = change_callback
            self._file_extensions = config.file_extensions

        def on_modified(self, event):
            if not isinstance(event, FileModifiedEvent):
                # Ignore directory modifications (e.g.: journal file being created and removed)
                return

            path = Path(event.src_path)
            suffix = path.suffix
            if suffix in self._file_extensions:
                self._callback(path)

    def __init__(self, config: Config, change_callback: Callable[[Path], None]):
        self._handler = Watcher.EventHandler(config, change_callback)
        self._observer = Observer()
        self._observer.schedule(self._handler, path=config.watch_directory, recursive=True)

    def __enter__(self):
        self._observer.start()

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._observer.stop()
        self._observer.join()
