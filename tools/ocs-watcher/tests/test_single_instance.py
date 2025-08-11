import pytest
import os
import tempfile
import time
import subprocess
import sys
from unittest.mock import patch

from ocsw.single_instance import SingleInstance


class TestSingleInstance:

    # noinspection PyAttributeOutsideInit
    def setup_method(self):
        self.lock_name = "ocsw-test.lock"
        self.temp_dir = tempfile.gettempdir()
        self.lockfile_path = os.path.join(self.temp_dir, self.lock_name)

    def teardown_method(self):
        if os.path.exists(self.lockfile_path):
            try:
                os.unlink(self.lockfile_path)
            except OSError:
                pass

    def check_lockfile_contains_current_pid(self):
        assert os.path.exists(self.lockfile_path)

        with open(self.lockfile_path, 'r') as f:
            pid = int(f.read().strip())

        assert pid == os.getpid()

    def test_single_instance_creation(self):
        with SingleInstance(self.lock_name) as instance:
            assert instance is not None
            self.check_lockfile_contains_current_pid()

    def test_lockfile_cleanup_on_exit(self):
        with SingleInstance(self.lock_name):
            assert os.path.exists(self.lockfile_path)

        assert not os.path.exists(self.lockfile_path)

    def test_second_instance_raises_error(self):
        with SingleInstance(self.lock_name):
            with pytest.raises(RuntimeError):
                with SingleInstance(self.lock_name):
                    pass

    def test_stale_lock_detection(self):
        fake_pid = 12345678

        with patch('psutil.pid_exists', return_value=False):
            with open(self.lockfile_path, 'w') as f:
                f.write(str(fake_pid))

            # Should be able to create instance despite existing lockfile
            with SingleInstance(self.lock_name):
                self.check_lockfile_contains_current_pid()

    def test_active_lock_prevents_new_instance(self):
        with open(self.lockfile_path, 'w') as f:
            f.write(str(os.getpid()))

        with pytest.raises(RuntimeError):
            with SingleInstance(self.lock_name):
                pass

    def test_corrupted_lockfile_handling(self):
        # Create corrupted lockfile
        with open(self.lockfile_path, 'w') as f:
            f.write("not_a_number")

        # Should be able to create instance despite corrupted lockfile
        with SingleInstance(self.lock_name):
            self.check_lockfile_contains_current_pid()

    def test_empty_lockfile_handling(self):
        # Create empty lockfile
        with open(self.lockfile_path, 'w'):
            pass

        # Should be able to create instance despite empty lockfile
        with SingleInstance(self.lock_name):
            self.check_lockfile_contains_current_pid()

    def test_exception_handling_in_context_manager(self):
        try:
            with SingleInstance(self.lock_name):
                assert os.path.exists(self.lockfile_path)
                raise ValueError("Test exception")
        except ValueError:
            pass

        # Lockfile should still be cleaned up despite exception
        assert not os.path.exists(self.lockfile_path)

    def test_multiple_different_instances(self):
        """Test that multiple instances with different names can coexist"""
        with SingleInstance("lock1"):
            assert os.path.exists(os.path.join(self.temp_dir, "lock1"))

            with SingleInstance("lock2"):
                assert os.path.exists(os.path.join(self.temp_dir, "lock1"))
                assert os.path.exists(os.path.join(self.temp_dir, "lock2"))

            assert os.path.exists(os.path.join(self.temp_dir, "lock1"))
            assert not os.path.exists(os.path.join(self.temp_dir, "lock2"))

        assert not os.path.exists(os.path.join(self.temp_dir, "lock1"))
        assert not os.path.exists(os.path.join(self.temp_dir, "lock2"))

    def test_real_subprocess_scenario(self):
        """Test with a real subprocess to simulate actual multi-instance scenario"""

        # Create a simple script that uses SingleInstance
        script_content = f'''
import sys
import os
import time
sys.path.insert(0, "{os.path.dirname(os.path.abspath(__file__))}")
from ocsw.single_instance import SingleInstance

try:
    with SingleInstance("{self.lock_name}"):
        time.sleep(2)
    print("SUCCESS")
except RuntimeError as e:
    print("ERROR:", str(e))
'''

        script_path = os.path.join(self.temp_dir, "test_script.py")
        with open(script_path, 'w') as f:
            f.write(script_content)

        try:
            # Start the first process
            process1 = subprocess.Popen([sys.executable, script_path],
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)

            # Give it time to acquire lock
            time.sleep(0.5)

            # Start the second process (should fail)
            process2 = subprocess.Popen([sys.executable, script_path],
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)

            # Wait for both to complete
            out1, err1 = process1.communicate()
            out2, err2 = process2.communicate()

            # First should succeed, second should fail
            assert "SUCCESS" in out1.decode()
            assert "ERROR:" in out2.decode()

        finally:
            if os.path.exists(script_path):
                os.unlink(script_path)
