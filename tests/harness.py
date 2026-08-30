#!/usr/bin/env python3
"""
Reusable pty harness for terminadventure integration tests.

Every test follows the same three-part shape:

    import harness

    s = harness.launch()     # <-- START: spawns the app, returns a Session
    try:
        s.send(b'a')         #     drive it like a real user...
        s.require('Alpha')   #     ...and assert what is on screen
    finally:
        s.quit()             # <-- END: quits the app (kills on failure)

`require`/`forbid` print the offending screen and exit non-zero on failure, so
`runtests.sh` only needs to check the exit code. The binary is resolved
relative to this file (../build/bin/terminadventure), so cwd does not matter;
override it with the environment variable TERMINADVENTURE_BIN.
"""

import fcntl
import os
import pty
import select
import shutil
import signal
import struct
import sys
import termios
import time

DEFAULT_BIN = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           '..', 'build', 'bin', 'terminadventure')


class Session:
    """A running terminadventure instance inside a pseudo-terminal."""

    def __init__(self, bin_path, cols=80, rows=24, workdir=None):
        self.bin_path = bin_path
        self.cols = cols
        self.rows = rows
        self.workdir = workdir or os.getcwd()
        self.master = None
        self.pid = None
        self.grid = [[(' ', set()) for _ in range(cols)] for _ in range(rows)]
        self._buf = b''
        self._r = 0
        self._c = 0
        self._sgr = set()
        self._utf8_pending = b''
        self._launch()
        self.step(timeout=1.0)  # let the first frame render

    # -- lifecycle ---------------------------------------------------------

    def _launch(self):
        master, slave = pty.openpty()
        fcntl.ioctl(slave, termios.TIOCSWINSZ,
                    struct.pack('HHHH', self.rows, self.cols, 0, 0))
        pid = os.fork()
        if pid == 0:
            os.setsid()
            fcntl.ioctl(slave, termios.TIOCSCTTY, 0)
            os.dup2(slave, 0)
            os.dup2(slave, 1)
            os.dup2(slave, 2)
            os.close(master)
            os.close(slave)
            os.chdir(self.workdir)
            os.environ['TERM'] = 'xterm-256color'
            # isolate init.conf (last-opened-file) per test workdir so a saved
            # file is auto-restored on relaunch instead of hitting build/bin/
            os.environ['TERMINADVENTURE_INIT'] = os.path.join(self.workdir, 'init.conf')
            os.execvp(self.bin_path, [self.bin_path])
            os._exit(127)
        os.close(slave)
        self.master = master
        self.pid = pid

    def quit(self):
        """Quit the app cleanly (:qa!), forcing a kill if it hangs.

        Uses the force-quit variant so teardown also works with unsaved
        changes, which would otherwise block :qa.

        Returns True when the app exited on its own, False when we had to
        kill it. Safe to call multiple times."""
        if self.pid is None:
            return True
        try:
            os.write(self.master, b':qa!\r')
        except OSError:
            pass
        self.step(timeout=0.5)
        deadline = time.time() + 2.0
        while time.time() < deadline:
            try:
                reaped, _ = os.waitpid(self.pid, os.WNOHANG)
                if reaped == self.pid:
                    self.pid = None
                    return True
            except ChildProcessError:
                self.pid = None
                return True
            self.step(timeout=0.2)
        try:
            os.kill(self.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        try:
            os.waitpid(self.pid, 0)
        except ChildProcessError:
            pass
        self.pid = None
        return False

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.quit()

    # -- input / screen ----------------------------------------------------

    def send(self, keys, wait=0.5):
        """Type `keys` (bytes or str) and repaint the screen model."""
        if isinstance(keys, str):
            keys = keys.encode()
        try:
            os.write(self.master, keys)
        except OSError:
            return
        self.step(timeout=wait)

    def step(self, timeout=0.5):
        """Drain pty output for `timeout` seconds into the screen model."""
        data = b''
        end = time.time() + timeout
        while time.time() < end:
            while select.select([self.master], [], [], 0)[0]:
                try:
                    chunk = os.read(self.master, 65536)
                except OSError:
                    return
                if not chunk:
                    return
                data += chunk
            time.sleep(0.02)
        self._feed(data)

    # -- assertions --------------------------------------------------------

    def find(self, substring, rows=None):
        """All (row, col) positions where `substring` appears on screen.

        `rows` (optional) restricts the search to a subset of screen rows;
        by default the whole screen is searched."""
        hits = []
        row_indices = range(self.rows) if rows is None else rows
        for r in row_indices:
            row = self._row_text(r)
            idx = row.find(substring)
            if idx >= 0:
                hits.append((r, idx))
        return hits

    def require(self, substring, msg=None):
        """Fail (dump + exit 1) unless `substring` is visible."""
        if not self.find(substring):
            print('FAIL: ' + (msg or 'missing %r on screen' % substring))
            self.dump()
            raise SystemExit(1)

    def forbid(self, substring, msg=None, rows=None):
        """Fail (dump + exit 1) if `substring` is visible.

        `rows` (optional) restricts the search to a subset of screen rows;
        by default the whole screen is searched."""
        if self.find(substring, rows=rows):
            print('FAIL: ' + (msg or 'unexpected %r on screen' % substring))
            self.dump()
            raise SystemExit(1)

    # -- inspection --------------------------------------------------------

    def row_text(self, r):
        return self._row_text(r)

    def screen(self):
        return [self._row_text(r) for r in range(self.rows)]

    def inv_rows(self, min_col=0, max_col=None):
        """Screen rows whose text is rendered inverted (selection/cursor)."""
        max_col = self.cols if max_col is None else max_col
        rows = []
        for r in range(self.rows):
            for c in range(min_col, min(max_col, self.cols)):
                if 'inv' in self.grid[r][c][1]:
                    rows.append(r)
                    break
        return rows

    def dump(self):
        for r in range(self.rows):
            print('%2d|%s|' % (r, self._row_text(r)))

    # -- internals ---------------------------------------------------------

    def _row_text(self, r):
        return ''.join(self.grid[r][c][0] for c in range(self.cols))

    def _feed(self, data):
        self._buf += data
        i = 0
        n = len(self._buf)
        while i < n:
            b = self._buf[i]
            if b == 0x1b:
                self._utf8_pending = b''
                if i + 1 >= n:
                    break
                if self._buf[i + 1] == 0x5b:
                    j = i + 2
                    while j < n and not (0x40 <= self._buf[j] <= 0x7e):
                        j += 1
                    if j >= n:
                        break
                    try:
                        self._handle_csi(
                            self._buf[i + 2:j].decode('ascii', 'replace'),
                            chr(self._buf[j]))
                    except Exception:
                        pass
                    i = j + 1
                else:
                    i += 2
            elif b == 0x0d:
                self._utf8_pending = b''
                self._c = 0
                i += 1
            elif b == 0x0a:
                self._utf8_pending = b''
                self._r = min(self._r + 1, self.rows - 1)
                i += 1
            elif b < 0x20:
                i += 1
            else:
                self._utf8_pending += bytes([b])
                i += 1
                try:
                    ch = self._utf8_pending.decode('utf-8')
                    self._utf8_pending = b''
                    self._put_char(ch)
                except UnicodeDecodeError:
                    if len(self._utf8_pending) >= 4:
                        self._utf8_pending = b''
                        self._put_char('?')
                except Exception:
                    pass
        self._buf = self._buf[i:]

    def _handle_csi(self, params, final):
        nums = []
        for p in params.split(';'):
            try:
                nums.append(int(p) if p else 0)
            except ValueError:
                nums.append(0)
        if final in 'Hf':
            row = (nums[0] or 1) - 1 if nums else 0
            col = (nums[1] if len(nums) > 1 and nums[1] else 1) - 1
            self._r = max(0, min(row, self.rows - 1))
            self._c = max(0, min(col, self.cols - 1))
        elif final == 'A':
            self._r = max(0, self._r - (nums[0] if nums else 1))
        elif final == 'B':
            self._r = min(self.rows - 1, self._r + (nums[0] if nums else 1))
        elif final == 'C':
            self._c = min(self.cols - 1, self._c + (nums[0] if nums else 1))
        elif final == 'D':
            self._c = max(0, self._c - (nums[0] if nums else 1))
        elif final == 'm':
            names = ['black', 'red', 'green', 'yellow', 'blue',
                     'magenta', 'cyan', 'white']
            i = 0
            if not nums or (len(nums) == 1 and nums[0] == 0):
                self._sgr = set()
            else:
                while i < len(nums):
                    code = nums[i]
                    if code == 0:
                        self._sgr = set()
                    elif code == 1:
                        self._sgr.add('bold')
                    elif code == 2:
                        self._sgr.add('dim')
                    elif code == 7:
                        self._sgr.add('inv')
                    elif code == 22:
                        self._sgr.discard('bold')
                    elif code == 27:
                        self._sgr.discard('inv')
                    elif 30 <= code <= 37:
                        self._sgr = {x for x in self._sgr if not x.startswith('fg')}
                        self._sgr.add('fg' + names[code - 30])
                    elif code == 39:
                        self._sgr = {x for x in self._sgr if not x.startswith('fg')}
                    elif 40 <= code <= 47:
                        self._sgr = {x for x in self._sgr if not x.startswith('bg')}
                        self._sgr.add('bg' + names[code - 40])
                    elif code == 49:
                        self._sgr = {x for x in self._sgr if not x.startswith('bg')}
                    elif 90 <= code <= 97:
                        self._sgr = {x for x in self._sgr if not x.startswith('fg')}
                        self._sgr.add('fg' + names[code - 90])
                    elif 100 <= code <= 107:
                        self._sgr = {x for x in self._sgr if not x.startswith('bg')}
                        self._sgr.add('bg' + names[code - 100])
                    elif code in (38, 48):
                        key = 'fg' if code == 38 else 'bg'
                        if i + 2 < len(nums) and nums[i + 1] == 5:
                            self._sgr = {x for x in self._sgr
                                         if not x.startswith(key)}
                            self._sgr.add(key + 'c%d' % nums[i + 2])
                            i += 2
                        elif i + 4 < len(nums) and nums[i + 1] == 2:
                            self._sgr = {x for x in self._sgr
                                         if not x.startswith(key)}
                            self._sgr.add(key + 'rgb%d_%d_%d'
                                          % (nums[i + 2], nums[i + 3], nums[i + 4]))
                            i += 4
                    i += 1
        elif final == 'J':
            self.grid = [[(' ', set()) for _ in range(self.cols)]
                         for _ in range(self.rows)]
            self._r = self._c = 0
        elif final == 'K':
            for c in range(self._c, self.cols):
                self.grid[self._r][c] = (' ', set())

    def _put_char(self, ch):
        if self._r < self.rows and self._c < self.cols:
            self.grid[self._r][self._c] = (ch, set(self._sgr))
        self._c += 1
        if self._c > self.cols:
            self._c = self.cols


def launch(bin_path=None, cols=80, rows=24, workdir=None):
    """START of a test: spawn the app and return a Session.

    With no workdir the test gets a fresh directory under
    /tmp/terminadventure_tests/ named after the test script (a previous run's
    files are removed). Pass an explicit workdir to keep it (e.g. to save
    a file and reload it in a second launch).
    """
    if bin_path is None:
        bin_path = os.environ.get('TERMINADVENTURE_BIN') or DEFAULT_BIN
    if not os.path.exists(bin_path):
        print('FAIL: binary not found at %s' % bin_path)
        print('      set TERMINADVENTURE_BIN or build the app first (./build.sh)')
        raise SystemExit(1)
    if workdir is None:
        script = os.path.basename(sys.argv[0] or 'test')
        name = os.path.splitext(script)[0]
        workdir = os.path.join('/tmp', 'terminadventure_tests', name)
        shutil.rmtree(workdir, ignore_errors=True)
    os.makedirs(workdir, exist_ok=True)
    return Session(bin_path, cols=cols, rows=rows, workdir=workdir)


def run(test_fn, **launch_kw):
    """Convenience: launch, run `test_fn(s)`, quit. Exits 1 on failure."""
    s = launch(**launch_kw)
    try:
        test_fn(s)
        s.quit()
        print('PASS')
    except SystemExit:
        s.quit()
        raise
    except Exception as exc:  # noqa: BLE001
        print('ERROR: %r' % exc)
        s.quit()
        raise


if __name__ == '__main__':
    print('harness.py is a library; write a test like:')
    print('  import harness')
    print('  s = harness.launch()')
    print('  s.require("Select a node to edit")')
    print('  s.quit()')
