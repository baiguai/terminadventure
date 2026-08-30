# Terminadventure
Terminadventure is a Vim-like notes editor that organizes plain text notes via a treeview structure.
So it has the familiar Vim modes plus a TREE mode for managing the treeview.

# Downloading


# Prerequisites

Terminadventure is a terminal app. Neither the Linux binary nor the
Windows `.exe` needs anything installed — no libraries, no runtimes,
no packages. What matters is that the **config files travel with the
binary**.

## Required: the three config files in the app`s directory

The app looks for `commands.conf`, `init.conf` and `terminadventure.html` in the
same folder as the executable (`build.sh` copies them there automatically). If
you hand someone just the binary, the app still runs, but with only the built-in
`:qa` command and no saved state.

- **commands.conf** — defines the key bindings and commands. Without it the app
  starts with nothing mapped except `:qa`.
- **init.conf** — holds your recent-files list and the last-opened document. It
  is written to (to persist those), so its folder must be **writable**. Missing
  or unwritable, it degrades gracefully: the app just won't remember anything.
  The path can be overridden with the `TERMINADVENTURE_INIT` environment variable.
- **terminadventure.html** — template used for HTML export.

## Linux binary

- Runs on any 64-bit desktop Linux. The dynamically-linked glibc/C++ runtime
  libraries are present on every normal install.
- Needs the **X11 client libraries** (libX11, libxcb, libXi, libXext), which
  FTXUI uses for clipboard support. These ship with any desktop Linux by
  default; you won't need to install anything on a machine with a GUI.
- Works in GNOME Terminal, Konsole, xterm, or any other terminal emulator.

## Windows `.exe`

- Self-contained static build — nothing to install.
- Runs on Windows 10/11 in **Windows Terminal**, the classic console, or any
  terminal emulator. The UI needs ANSI escape-sequence support, which the
  default console host on Windows 10+ provides automatically.

## For developers building from source

Building (not just running) has extra requirements; see `./setup.sh`, which
checks and reports them (cmake 3.16+, a C++17 compiler, make, git, network to
fetch FTXUI).
