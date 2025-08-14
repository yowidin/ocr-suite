import os
import atexit
import tempfile
import psutil


class SingleInstance:
    def __init__(self, lock_name="ocsw.lock"):
        self.lockfile_path = os.path.join(tempfile.gettempdir(), lock_name)
        self.pid = os.getpid()

    def __enter__(self):
        if self.is_running():
            raise RuntimeError("Another instance is already running")

        with open(self.lockfile_path, 'w') as f:
            f.write(str(self.pid))

        atexit.register(self._cleanup)

        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._cleanup()

    def is_running(self):
        if not os.path.exists(self.lockfile_path):
            return False

        try:
            with open(self.lockfile_path, 'r') as f:
                pid = int(f.read().strip())

            return psutil.pid_exists(pid)
        except (OSError, ValueError):
            self._cleanup()
            return False

    def _cleanup(self):
        try:
            if os.path.exists(self.lockfile_path):
                with open(self.lockfile_path, 'r') as f:
                    pid = int(f.read().strip())

                if pid == self.pid:
                    os.unlink(self.lockfile_path)
        except (OSError, ValueError):
            pass
