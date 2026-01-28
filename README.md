# Vrdg - Minimal Render Graph Generator

[![CI](https://github.com/hypernewbie/vrdg/actions/workflows/ci.yml/badge.svg)](https://github.com/hypernewbie/vrdg/actions/workflows/ci.yml)

Vrdg is a minimal render dependency graph Python code generator. It lets you describe render/compute tasks using `///>` annotations in a C++ source file, generates the wiring code into a header, and then runs the tasks through a minimal runtime (`vrdg.h`) that handles task ordering, resource transitions, and releases.

> NOTE: VRDG contains more heavily coded testing infra. Read at your own risk. The core vrdg.py and vrdg.h parts are mostly human written.

Requirements
- CMake 3.10+
- A C++11 compiler
- Python 3 (CMake uses `find_package(Python3 REQUIRED)`)

# Building & Running Tests

Build
1) Configure:
- Windows:
```sh
cmake -S . -B build
```

- macOS/Linux:
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

2) Build:
- Windows:
```sh
cmake --build build --config Debug
```

- macOS/Linux:
```sh
cmake --build build
```

Run the example
- Windows:
```sh
build/Debug/vrdg_test.exe
```

- macOS/Linux:
```sh
build/vrdg_test
```

Run the verification script
- Windows:
```sh
py verify.py
```

- macOS/Linux:
```sh
python3 verify.py
```

Run CTest
- Windows / Visual Studio:
```sh
cd build
ctest -C Debug -V
```

- macOS/Linux:
```sh
cd build
ctest -V
```

If CTest reports no tests or a missing configuration:
- Re-run the build so CMake regenerates after changes to `CMakeLists.txt`.
- Use `-C Debug` (or your chosen config) with `ctest` for multi-config generators.

How the generator works
`vrdg.py` scans `test.cpp` for `///>` directives like:
```cpp
///> Graphics_Task( G1 )
///>     Write_Buffer( a )
///>     Read_TextureFB( c )
///>     Func( G1_Func )
///> End_Task
```

At build time, CMake runs:
```sh
${Python3_EXECUTABLE} vrdg.py test.cpp > test.h
```

The generated `test.h` provides task functions, callback bodies, and resource wiring that are consumed by `test.cpp` and the runtime in `vrdg.h`.

Notes
- The build uses a custom command/target to regenerate `test.h` whenever `test.cpp` or `vrdg.py` changes.
- `verify.py` expects the executable at `build/Debug/vrdg_test.exe` on Windows.
