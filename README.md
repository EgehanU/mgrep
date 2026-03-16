# mgrep — multithreaded grep

A fast, multithreaded file search tool written in C++17. Searches recursively
through a directory tree using a thread pool, one thread per file. Good for
codebases where file I/O is the bottleneck and you want to parallelize it.

## Features

- Thread pool with one worker per logical CPU core
- Recursive directory traversal via `std::filesystem`
- Plain string search (fast) or extended regex via `std::regex`
- Case-insensitive mode
- Binary file detection (skips them automatically)
- Colored output with match highlighting
- Extension filtering (`--ext=.cpp`)
- Match count mode (`-c`)
- Timing stats printed at the end

## Build

```bash
make
```

Requires g++ with C++17 and pthreads. Tested on Linux (Ubuntu 22.04+).

```bash
make debug   # build with AddressSanitizer + ThreadSanitizer
```

## Usage

```
./mgrep [options] <pattern> <path>

Options:
  -i           case insensitive
  -E           treat pattern as extended regex
  -c           count matches only
  -n           hide line numbers
  --no-color   disable colors
  --ext=EXT    only search files with this extension (can repeat)
  --depth=N    max directory depth
```

## Examples

```bash
# find all TODOs in C++ source files
./mgrep -i "todo" ./src --ext=.cpp --ext=.h

# find all TODO or FIXME using regex
./mgrep -E "TODO|FIXME" ./src

# count how many times a function is called across the project
./mgrep -c "connect(" ./src

# search only 2 levels deep
./mgrep --depth=2 "main" .
```

## Architecture

```
main.cpp        — arg parsing, output, timing
searcher.h/cpp  — directory walk + per-file search logic
thread_pool.h   — generic work-stealing thread pool
result.h        — Match and FileResult structs, SearchConfig
```

The directory walk happens on the main thread and enqueues file search tasks
into the thread pool. Each worker pulls tasks from the shared queue (protected
by a mutex + condition variable). Results are collected in a shared vector with
a separate mutex. After all workers finish (`waitAll()`), results are sorted by
filename for deterministic output.

## Performance

On a machine with 8 logical cores searching a mid-sized codebase (~5000 files),
mgrep is typically 4-6x faster than single-threaded grep for I/O-bound cases.
For small codebases the threading overhead dominates and it won't be faster.
