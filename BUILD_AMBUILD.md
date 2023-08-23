Building Evobot with AMBuild
===============

# Requirements

## Windows

* Visual C++ 2017 or higher.
* Python 3.3 or higher.
* [Git]
* [AMBuild]

## Linux

* Python 3.3 or higher.
* A C++ compiler such as gcc or clang.
* If building on a 64-bit system, you must install the 32-bit binaries for the compiler.
* Install git from your system package manager.
* [AMBuild]

# Building

After cloning the current repo, you must configure the build.

First create a folder called **build** and then run *configure.py*. Example:

```
cd evobot_mm
mkdir build
cd build
python ../configure.py
```

You may also need to run Python as `python3` instead of `python` on some systems.

It is safe to reconfigure over an old build. However, it's probably a bad idea to configure inside a random, non-empty folder.

There are a few extra options you can pass to *configure*:

* --enable-optimize - Compile with optimization.
* --enable-debug - Compile with symbols and debug checks.
* --enable-static-lib - Statically link the sanitizer runtime (Linux only).
* --enable-shared-lib - Dynamically link the sanitizer runtime (Linux only).
* --symbol-files - Split debugging symbols from binaries into separate symbol files.

To build, simply type: `ambuild`

[AMBuild]: https://wiki.alliedmods.net/Ambuild
[Git]: https://git-scm.com/

All thanks to Anonymous Player for writing the build script and configuration.