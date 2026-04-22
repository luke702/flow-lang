# Flow (v0) — C reference interpreter

This directory contains a **C** implementation of a small **Flow** subset (lexer, parser, tree-walking interpreter).

## Build

**Windows (GCC / MSYS2 MinGW, recommended here):** compile each translation unit separately, then link (some MinGW builds mis-assemble when all `.c` files are passed in one command):

```bat
build.bat
```

Or manually:

```bat
mkdir build 2>nul
gcc -std=c11 -O2 -I include -c src\main.c -o build\main.o
gcc -std=c11 -O2 -I include -c src\lexer.c -o build\lexer.o
gcc -std=c11 -O2 -I include -c src\parse.c -o build\parse.o
gcc -std=c11 -O2 -I include -c src\ast.c -o build\ast.o
gcc -std=c11 -O2 -I include -c src\value.c -o build\value.o
gcc -std=c11 -O2 -I include -c src\interp.c -o build\interp.o
gcc -std=c11 -O2 -I include -c src\pathutil.c -o build\pathutil.o
gcc -o flow.exe build\main.o build\lexer.o build\parse.o build\ast.o build\value.o build\interp.o build\pathutil.o
```

**CMake** (if `cmake` is on your `PATH`):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**GNU Make** (if `make` is available):

```bash
make
```

The executable is `flow` or `flow.exe` in the project root (or under `build/` when using CMake).

## Install / package

```bash
cmake --install build --prefix dist
```

This copies the `flow` binary to `dist/bin`. Zip that folder for a portable binary package.

## Program entry (`main`)

Every `.flow` file must define **exactly one** top-level `func main()` with **no parameters**. The interpreter loads imports first, then registers other top-level `func` / `let` / `library`, then **calls `main()` once**. Top-level expression statements (e.g. bare `println(...)`) are **not** allowed—put them inside `main`.

Imported modules follow the same rule (one `main`) but **`main` is not called** when loading a library.

## Run demo

```bash
./build/flow examples/demo.flow
```

Expected output:

```text
Flow
42
4
ok
[2, 4, 6]
```

## Libraries (`import` / `export` / `library`)

- **`library "name";`** — optional metadata at top level (no runtime effect).
- **`import "path.flow";`** — loads another file; bindings are **not** visible unless exported.
- **`export func …` / `export let …`** — only top-level exports are merged into the importer’s environment.
- Resolution: path relative to the **current script’s directory**, then each entry in **`FLOW_PATH`** (use `;` as separator on Windows, `:` on Unix).

Builtin commands:

```bash
flow libs
```

The library used by the demo lives under `examples/lib/` (e.g. `examples/lib/math.flow`).

Imported modules keep their syntax tree in memory for the lifetime of the process so that exported functions remain callable.

## HTTP GET

Builtin `http_get(url)` runs **`curl`** (must be on `PATH`; Windows 10+ includes `curl.exe`) and returns the response body as a string.

The same `examples/demo.flow` also calls `http_get` at the end (needs network).

## Notes

- Statement terminators `;` are **optional** everywhere (you may omit them before another statement or `}`).
- `int / int` uses truncating integer division (toward zero), matching the language design.
- `list.map(lambda)` is implemented as a special case on the interpreter (method-style call).

See `docs/flow-v0-spec-and-semantics.md` for the broader design context.
