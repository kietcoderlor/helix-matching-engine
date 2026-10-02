# Build

Helix is C++20. The matching engine builds with CMake 3.20+.

## Windows (this development machine)

Visual Studio 18 2026 + MSVC. From the repository root:

```powershell
cmake -B build -S . -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\engine\Release\helix_engine.exe
```

Debug:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Linux

`epoll` is the intended production poller. A typical command:

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Address/UB sanitizers (Unix only):

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DHELIX_ENABLE_SANITIZERS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## What is not required yet

Gateway, PostgreSQL, Docker, and the React terminal are later milestones.
Do not start them until `docs/status.md` says they exist.
