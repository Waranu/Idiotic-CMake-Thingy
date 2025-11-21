#!/usr/bin/env python3
"""
build.py - Windows-friendly version

Usage examples:
  python build.py                 # configure only (default)
  python build.py -b              # configure + build
  python build.py -r              # build + run
  python build.py -b -r           # configure + build + run
  python build.py -c -b -r        # clean then configure + build + run
  python build.py -b -r --compact # same, but shrink console window on Windows
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

IS_WINDOWS = os.name == "nt"


def eprint(*a, **k):
    print(*a, file=sys.stderr, **k)


def set_compact_console(cols=100, lines=30):
    """Make the console window compact on Windows (no-op on other platforms)."""
    if not IS_WINDOWS:
        return
    # Try using 'mode' which is simpler and broadly available
    try:
        os.system(f"mode con: cols={cols} lines={lines}")
    except Exception:
        pass


def run_cmd_live(cmd, cwd=None):
    """
    Run a command while streaming its stdout/stderr to the terminal (no capture).
    Raises subprocess.CalledProcessError on non-zero exit.
    """
    # Print the command for clarity
    print("\n$ " + " ".join(cmd))
    try:
        # use shell=False; let output flow to parent's console
        completed = subprocess.run(cmd, cwd=cwd)
    except FileNotFoundError as exc:
        eprint(f"ERROR: Command not found: {cmd[0]}")
        raise
    except Exception as exc:
        eprint(f"ERROR: Running command failed: {exc}")
        raise

    if completed.returncode != 0:
        eprint(
            f"\nCommand {' '.join(cmd)} failed with exit code {completed.returncode}"
        )
        raise subprocess.CalledProcessError(completed.returncode, cmd)

    return completed


def require_tool(name):
    """Exit if tool not found in PATH."""
    path = shutil.which(name)
    if path is None:
        eprint(f"ERROR: Required tool '{name}' not found in PATH. Aborting.")
        sys.exit(2)
    return path


def main():
    parser = argparse.ArgumentParser(
        description="Configure/build/run (Ninja + CMake + clang-cl) build directory."
    )
    parser.add_argument(
        "-c",
        "--clean",
        action="store_true",
        help="Clean current directory (exclude this script).",
    )
    parser.add_argument(
        "-b",
        "--build",
        action="store_true",
        help="Run CMake configure and ninja build.",
    )
    parser.add_argument(
        "-r",
        "--run",
        action="store_true",
        help="Run ninja (if needed) and launch the first .exe in current directory.",
    )
    parser.add_argument(
        "--compact",
        action="store_true",
        help="(Windows) make the console window compact (smaller).",
    )
    args = parser.parse_args()

    # If compact requested, make console small (Windows-only)
    if args.compact and IS_WINDOWS:
        set_compact_console(cols=100, lines=30)

    # Name of this script so we don't delete it during clean
    script_name = Path(sys.argv[0]).name if sys.argv[0] else "__build_script__"
    cwd = Path.cwd()

    # ---------- 1) CLEAN ----------
    if args.clean:
        print(f"🧹 Cleaning directory (excluding {script_name})...")
        for entry in list(cwd.iterdir()):
            if entry.name == script_name:
                continue
            try:
                if entry.is_dir():
                    shutil.rmtree(entry)
                else:
                    entry.unlink()
            except Exception as exc:
                eprint(f"Warning: could not remove {entry}: {exc}")
        print("✅ Clean complete.\n")

    # Determine if we need to run ninja at all (build or run)
    need_ninja = args.build or args.run

    # ---------- tool sanity checks ----------
    # Configure is run when -b or (default: no -b and no -r)
    should_configure = args.build or (not need_ninja)

    if should_configure:
        require_tool("cmake")

    if need_ninja:
        require_tool("ninja")
        require_tool("clang-cl")  # assert clang-cl is available (we set it as compiler)

    # ---------- 2) CONFIGURE ----------
    if should_configure:
        print("⚙️  Running CMake configure...")
        cmake_cmd = [
            "cmake",
            "..",
            "-G",
            "Ninja",
            "-DCMAKE_C_COMPILER=clang-cl",
            "-DCMAKE_CXX_COMPILER=clang-cl",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        ]
        try:
            run_cmd_live(cmake_cmd, cwd=str(cwd))
        except FileNotFoundError:
            eprint("CMake executable not found while running. Aborting.")
            sys.exit(2)
        except subprocess.CalledProcessError:
            eprint("CMake configuration failed. Aborting.")
            sys.exit(3)

    # ---------- 3) BUILD ----------
    if need_ninja:
        print("\n🔨 Building with ninja...")
        try:
            run_cmd_live(["ninja"], cwd=str(cwd))
        except FileNotFoundError:
            eprint("ninja executable not found while running. Aborting.")
            sys.exit(2)
        except subprocess.CalledProcessError:
            eprint("ninja build failed. Aborting.")
            sys.exit(4)

    # ---------- 4) RUN ----------
    if args.run:
        print("\n🔍 Scanning for .exe in", cwd, "...")
        exe_list = [
            p for p in cwd.iterdir() if p.is_file() and p.suffix.lower() == ".exe"
        ]
        if not exe_list:
            eprint("ERROR: No executable (*.exe) found in current directory!")
            sys.exit(5)

        exe = exe_list[0]
        print(f"🚀 Launching {exe.name} ...")

        try:
            if IS_WINDOWS:
                # On Windows, launching the exe directly is fine; let it inherit the current console.
                # If you want it in its own new console window, uncomment creationflags below:
                # creationflags = subprocess.CREATE_NEW_CONSOLE
                run_cmd_live([str(exe)], cwd=str(cwd))
            else:
                run_cmd_live([str(exe)], cwd=str(cwd))
        except subprocess.CalledProcessError:
            eprint(f"Executable {exe.name} exited with non-zero status.")
            sys.exit(6)
        except Exception as exc:
            eprint(f"Failed to launch {exe.name}: {exc}")
            sys.exit(7)

    print("\n✅ Done.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        eprint("\nInterrupted by user.")
        raise SystemExit(130)
